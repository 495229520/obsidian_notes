#include "mfms/server_handler.hpp"
#include "nlohmann/json.hpp"
#include <spdlog/spdlog.h>
#include <unistd.h>

namespace mfms {

ServerHandler::ServerHandler(const Config& config)
    : config_(config)
    , checkpoint_interval_(config.db_checkpoint_interval_sec)
{}

ServerHandler::~ServerHandler() {
    stop();
}

bool ServerHandler::init() {
    // Initialize components
    file_manager_ = std::make_unique<fs::FileManager>(config_);
    if (!file_manager_->init()) {
        spdlog::error("Failed to initialize FileManager");
        return false;
    }

    codec_registry_ = std::make_unique<codec::CodecRegistry>();
    status_cache_ = std::make_unique<reactor::StatusCache>(config_.snapshot_refresh_cooldown_ms);

    reactor_ = std::make_unique<reactor::Reactor>(config_);
    if (!reactor_->init()) {
        spdlog::error("Failed to initialize Reactor");
        return false;
    }
    reactor_->setHandler(this);

    db_bridge_ = std::make_unique<db_bridge::DbBridge>(config_);
    if (!db_bridge_->start(reactor_.get())) {
        spdlog::error("Failed to start DbBridge");
        return false;
    }

    ros_bridge_ = std::make_unique<ros_bridge::RosBridge>(config_);
    if (!ros_bridge_->start(reactor_.get())) {
        spdlog::error("Failed to start RosBridge");
        return false;
    }

    // Cleanup old temp files
    file_manager_->cleanupTempFiles();

    spdlog::info("ServerHandler initialized");
    return true;
}

void ServerHandler::start() {
    spdlog::info("ServerHandler starting...");
    reactor_->run();
}

void ServerHandler::stop() {
    if (reactor_) reactor_->stop();
    if (ros_bridge_) ros_bridge_->stop();
    if (db_bridge_) db_bridge_->stop();
}

void ServerHandler::onFrame(reactor::Session& session, const protocol::FrameHeader& hdr,
                            std::vector<std::uint8_t> payload) {
    ++stats_.frames_received;
    stats_.bytes_received += protocol::kHeaderSize + payload.size();

    spdlog::debug("Client {}: received frame type={:#x} req_id={} payload_len={}",
                  session.id(), static_cast<uint16_t>(hdr.msg_type),
                  hdr.request_id, hdr.payload_len);

    switch (hdr.msg_type) {
        case protocol::MsgType::STATUS_SNAPSHOT_REQ:
            handleStatusSnapshotReq(session, hdr);
            break;
        case protocol::MsgType::MOTION_REQ:
            handleMotionReq(session, hdr, payload);
            break;
        case protocol::MsgType::LUA_UPLOAD_BEGIN:
            handleUploadBegin(session, hdr, payload, true);
            break;
        case protocol::MsgType::QT_LOG_BEGIN:
            handleUploadBegin(session, hdr, payload, false);
            break;
        case protocol::MsgType::LUA_UPLOAD_CHUNK:
        case protocol::MsgType::QT_LOG_CHUNK:
            handleUploadChunk(session, hdr, payload);
            break;
        case protocol::MsgType::LUA_UPLOAD_END:
        case protocol::MsgType::QT_LOG_END:
            handleUploadEnd(session, hdr, payload);
            break;
        case protocol::MsgType::SERVER_LOG_REQ:
            handleServerLogReq(session, hdr, payload);
            break;
        default:
            ++stats_.protocol_errors;
            spdlog::warn("Client {}: unknown message type {:#x}",
                         session.id(), static_cast<uint16_t>(hdr.msg_type));
            reactor_->sendError(session.id(), hdr.request_id,
                                ErrorCode::UNKNOWN_MSG_TYPE, "Unknown message type");
            break;
    }
}

void ServerHandler::onDisconnect(reactor::Session& session, const std::string& reason) {
    spdlog::info("Client {} disconnected: {}", session.id(), reason);

    // Log event to DB
    db_bridge::EventRecord event;
    event.type = db_bridge::EventType::CLIENT_DISCONNECTED;
    event.entity_id = std::to_string(session.id());
    event.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        Clock::now().time_since_epoch()).count();
    event.details_json = nlohmann::json({{"reason", reason}, {"remote_addr", session.remoteAddr()}}).dump();
    db_bridge_->enqueueEvent(std::move(event));
}

void ServerHandler::onRosStatus(const reactor::RosStatusUpdate& update) {
    spdlog::trace("ROS status: robot={} type={}", update.robot_id, update.robot_type);

    // Decode using codec registry
    codec::RobotStatusEnvelope envelope;
    envelope.robot_id = update.robot_id;
    envelope.robot_type = update.robot_type;
    envelope.timestamp_ns = update.timestamp_ns;
    envelope.raw_payload = update.raw_payload;

    auto result = codec_registry_->decode(envelope);
    if (result.success) {
        status_cache_->updateStatus(update.robot_id, std::move(result.data));
    } else {
        spdlog::warn("Failed to decode status for robot {}: {}", update.robot_id, result.message);
    }
}

void ServerHandler::onRosServiceComplete(const reactor::RosServiceCompletion& completion) {
    spdlog::debug("ROS service complete: req_id={} robot={} success={}",
                  completion.request_id, completion.robot_id, completion.success);

    // Find the client session and send response
    auto* session = reactor_->getSession(completion.client_id);
    if (!session) {
        spdlog::warn("Service completion for unknown client {}", completion.client_id);
        return;
    }

    // Remove from in-flight
    session->removeInflight(completion.request_id);

    // Send response
    protocol::MotionResponsePayload resp;
    resp.robot_id = completion.robot_id;
    resp.success = completion.success;
    resp.error_code = completion.error_code;
    resp.result = completion.result_json;

    std::vector<uint8_t> payload;
    resp.serialize(payload);

    protocol::FrameHeader hdr;
    hdr.msg_type = protocol::MsgType::MOTION_RESP;
    hdr.request_id = completion.request_id;
    hdr.payload_len = static_cast<uint32_t>(payload.size());

    reactor_->sendResponse(completion.client_id, hdr, payload);

    // Log to DB
    db_bridge::EventRecord event;
    event.type = db_bridge::EventType::SERVICE_RESULT;
    event.entity_id = completion.robot_id;
    event.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        Clock::now().time_since_epoch()).count();
    event.details_json = nlohmann::json({
        {"request_id", completion.request_id},
        {"client_id", completion.client_id},
        {"success", completion.success},
        {"error_code", completion.error_code}
    }).dump();
    db_bridge_->enqueueEvent(std::move(event));
}

void ServerHandler::onTimerTick() {
    // Rebuild status snapshot if needed
    status_cache_->maybeRebuild();

    // Checkpoint robot states to DB periodically
    checkpointRobotStates();

    // Generate daily report at midnight
    maybeGenerateDailyReport();
}

void ServerHandler::handleStatusSnapshotReq(reactor::Session& session,
                                            const protocol::FrameHeader& hdr) {
    auto snapshot = status_cache_->getSnapshot();

    protocol::FrameHeader resp_hdr;
    resp_hdr.msg_type = protocol::MsgType::STATUS_SNAPSHOT_RESP;
    resp_hdr.request_id = hdr.request_id;
    resp_hdr.payload_len = static_cast<uint32_t>(snapshot->size());

    std::vector<uint8_t> payload(snapshot->begin(), snapshot->end());
    reactor_->sendResponse(session.id(), resp_hdr, payload);

    spdlog::debug("Client {}: sent snapshot ({} bytes)", session.id(), snapshot->size());
}

void ServerHandler::handleMotionReq(reactor::Session& session,
                                    const protocol::FrameHeader& hdr,
                                    const std::vector<std::uint8_t>& payload) {
    ++stats_.motion_requests;

    protocol::MotionRequestPayload req;
    if (!protocol::MotionRequestPayload::deserialize(payload.data(), payload.size(), req)) {
        reactor_->sendError(session.id(), hdr.request_id,
                            ErrorCode::INVALID_FRAME, "Invalid motion request payload");
        return;
    }

    // Check if we can add to in-flight
    reactor::InflightRequest inflight;
    inflight.request_id = hdr.request_id;
    inflight.msg_type = hdr.msg_type;
    inflight.sent_at = Clock::now();
    inflight.robot_id = req.robot_id;

    if (!session.addInflight(inflight)) {
        reactor_->sendError(session.id(), hdr.request_id,
                            ErrorCode::TOO_MANY_REQUESTS, "Too many in-flight requests");
        return;
    }

    // Forward to ROS
    if (!ros_bridge_->sendCommand(hdr.request_id, session.id(),
                                  req.robot_id, req.command, req.timeout_ms)) {
        session.removeInflight(hdr.request_id);
        reactor_->sendError(session.id(), hdr.request_id,
                            ErrorCode::ROBOT_NOT_FOUND, "Robot not available");
    }
}

void ServerHandler::handleUploadBegin(reactor::Session& session,
                                      const protocol::FrameHeader& hdr,
                                      const std::vector<std::uint8_t>& payload,
                                      bool is_lua) {
    if (session.hasActiveUpload()) {
        reactor_->sendError(session.id(), hdr.request_id,
                            ErrorCode::UPLOAD_ALREADY_IN_PROGRESS, "Upload already in progress");
        return;
    }

    protocol::UploadBeginPayload begin;
    if (!protocol::UploadBeginPayload::deserialize(payload.data(), payload.size(), begin)) {
        reactor_->sendError(session.id(), hdr.request_id,
                            ErrorCode::INVALID_FRAME, "Invalid upload begin payload");
        return;
    }

    // Validate
    if (begin.total_size > config_.max_upload_size) {
        reactor_->sendError(session.id(), hdr.request_id,
                            ErrorCode::UPLOAD_TOO_LARGE, "Upload too large");
        return;
    }

    std::string safe_filename;
    if (!protocol::sanitizeFilename(begin.filename, safe_filename)) {
        reactor_->sendError(session.id(), hdr.request_id,
                            ErrorCode::INVALID_FILENAME, "Invalid filename");
        return;
    }

    // Create temp file
    std::string temp_path;
    int fd = file_manager_->createTempFile(is_lua ? "lua" : "qtlog", temp_path);
    if (fd < 0) {
        reactor_->sendError(session.id(), hdr.request_id,
                            ErrorCode::INTERNAL_ERROR, "Failed to create temp file");
        return;
    }

    // Start upload state
    reactor::UploadState state;
    state.upload_id = begin.upload_id;
    state.upload_type = is_lua ? protocol::MsgType::LUA_UPLOAD_BEGIN : protocol::MsgType::QT_LOG_BEGIN;
    state.filename = safe_filename;
    state.target_robot_id = begin.target_robot_id;
    state.temp_path = temp_path;
    state.total_size = begin.total_size;
    state.received_size = 0;
    state.expected_crc32 = begin.file_crc32;
    state.temp_fd = fd;
    state.started_at = Clock::now();
    state.last_chunk_at = Clock::now();

    session.startUpload(std::move(state));

    reactor_->sendAck(session.id(), hdr.request_id);
    spdlog::info("Client {}: started {} upload: {} ({} bytes)",
                 session.id(), is_lua ? "LUA" : "QT_LOG", safe_filename, begin.total_size);
}

void ServerHandler::handleUploadChunk(reactor::Session& session,
                                      const protocol::FrameHeader& hdr,
                                      const std::vector<std::uint8_t>& payload) {
    if (!session.hasActiveUpload()) {
        reactor_->sendError(session.id(), hdr.request_id,
                            ErrorCode::UPLOAD_NOT_STARTED, "No upload in progress");
        return;
    }

    protocol::UploadChunkPayload chunk;
    if (!protocol::UploadChunkPayload::deserialize(payload.data(), payload.size(), chunk)) {
        reactor_->sendError(session.id(), hdr.request_id,
                            ErrorCode::INVALID_FRAME, "Invalid chunk payload");
        return;
    }

    auto& state = session.uploadState();

    // Validate upload_id and offset
    if (chunk.upload_id != state.upload_id) {
        reactor_->sendError(session.id(), hdr.request_id,
                            ErrorCode::INVALID_FRAME, "Upload ID mismatch");
        return;
    }

    if (chunk.offset != state.received_size) {
        reactor_->sendError(session.id(), hdr.request_id,
                            ErrorCode::UPLOAD_CHUNK_OUT_OF_ORDER, "Chunk out of order");
        return;
    }

    // Bounds check: ensure chunk won't exceed total_size
    if (state.received_size + chunk.data.size() > state.total_size) {
        spdlog::warn("Client {}: chunk would exceed total_size", session.id());
        session.clearUpload();
        reactor_->sendError(session.id(), hdr.request_id,
                            ErrorCode::UPLOAD_SIZE_MISMATCH, "Chunk exceeds declared size");
        return;
    }

    // Write chunk to temp file
    ssize_t written = write(state.temp_fd, chunk.data.data(), chunk.data.size());
    if (written < 0 || static_cast<size_t>(written) != chunk.data.size()) {
        spdlog::error("Client {}: write to temp file failed", session.id());
        session.clearUpload();
        reactor_->sendError(session.id(), hdr.request_id,
                            ErrorCode::INTERNAL_ERROR, "Write failed");
        return;
    }

    state.received_size += static_cast<uint32_t>(chunk.data.size());
    state.last_chunk_at = Clock::now();

    reactor_->sendAck(session.id(), hdr.request_id);
}

void ServerHandler::handleUploadEnd(reactor::Session& session,
                                    const protocol::FrameHeader& hdr,
                                    const std::vector<std::uint8_t>& payload) {
    if (!session.hasActiveUpload()) {
        reactor_->sendError(session.id(), hdr.request_id,
                            ErrorCode::UPLOAD_NOT_STARTED, "No upload in progress");
        return;
    }

    protocol::UploadEndPayload end;
    if (!protocol::UploadEndPayload::deserialize(payload.data(), payload.size(), end)) {
        reactor_->sendError(session.id(), hdr.request_id,
                            ErrorCode::INVALID_FRAME, "Invalid end payload");
        return;
    }

    auto& state = session.uploadState();

    // Validate
    if (end.upload_id != state.upload_id) {
        reactor_->sendError(session.id(), hdr.request_id,
                            ErrorCode::INVALID_FRAME, "Upload ID mismatch");
        return;
    }

    if (end.final_size != state.received_size) {
        spdlog::warn("Client {}: upload size mismatch: expected={} received={}",
                     session.id(), end.final_size, state.received_size);
        ++stats_.uploads_failed;
        session.clearUpload();
        reactor_->sendError(session.id(), hdr.request_id,
                            ErrorCode::UPLOAD_SIZE_MISMATCH, "Size mismatch");
        return;
    }

    // Determine final path
    std::string final_path;
    bool is_lua = (state.upload_type == protocol::MsgType::LUA_UPLOAD_BEGIN);
    if (is_lua) {
        final_path = file_manager_->getLuaDir(state.target_robot_id) + "/" + state.filename;
    } else {
        final_path = file_manager_->getQtLogDir(std::to_string(session.id())) + "/" + state.filename;
    }

    // Get temp_path from state (already stored)
    std::string temp_path = state.temp_path;
    int temp_fd = state.temp_fd;

    // Mark fd/path as handled to prevent double-close in clearUpload
    state.temp_fd = -1;
    state.temp_path.clear();

    // Finalize
    if (file_manager_->finalizeUpload(temp_fd, temp_path, final_path)) {
        ++stats_.uploads_completed;
        spdlog::info("Client {}: upload complete: {}", session.id(), final_path);

        // If LUA, forward to robot
        if (is_lua && !state.target_robot_id.empty()) {
            ros_bridge_->sendLuaScript(hdr.request_id, session.id(),
                                       state.target_robot_id, final_path);
        }

        session.clearUpload();
        reactor_->sendAck(session.id(), hdr.request_id);
    } else {
        ++stats_.uploads_failed;
        session.clearUpload();
        reactor_->sendError(session.id(), hdr.request_id,
                            ErrorCode::INTERNAL_ERROR, "Finalize failed");
    }
}

void ServerHandler::handleServerLogReq(reactor::Session& session,
                                       const protocol::FrameHeader& hdr,
                                       const std::vector<std::uint8_t>& payload) {
    protocol::ServerLogRequestPayload req;
    if (!protocol::ServerLogRequestPayload::deserialize(payload.data(), payload.size(), req)) {
        reactor_->sendError(session.id(), hdr.request_id,
                            ErrorCode::INVALID_FRAME, "Invalid log request payload");
        return;
    }

    std::string content = file_manager_->readServerLog(req.date, req.offset, req.max_size);

    if (content.empty()) {
        protocol::FrameHeader end_hdr;
        end_hdr.msg_type = protocol::MsgType::SERVER_LOG_END;
        end_hdr.request_id = hdr.request_id;
        end_hdr.payload_len = 0;
        reactor_->sendResponse(session.id(), end_hdr, {});
    } else {
        // Send as single chunk for simplicity
        protocol::FrameHeader chunk_hdr;
        chunk_hdr.msg_type = protocol::MsgType::SERVER_LOG_CHUNK;
        chunk_hdr.request_id = hdr.request_id;
        chunk_hdr.payload_len = static_cast<uint32_t>(content.size());

        std::vector<uint8_t> chunk_payload(content.begin(), content.end());
        reactor_->sendResponse(session.id(), chunk_hdr, chunk_payload);

        protocol::FrameHeader end_hdr;
        end_hdr.msg_type = protocol::MsgType::SERVER_LOG_END;
        end_hdr.request_id = hdr.request_id;
        end_hdr.payload_len = 0;
        reactor_->sendResponse(session.id(), end_hdr, {});
    }
}

void ServerHandler::checkpointRobotStates() {
    auto now = Clock::now();

    // This would iterate through all robots in status_cache and checkpoint
    // those that haven't been checkpointed recently
    // For now, simplified implementation
}

void ServerHandler::maybeGenerateDailyReport() {
    std::string today = fs::FileManager::todayDateStr();
    if (today == last_report_date_) {
        return;
    }

    // Generate report for previous day
    last_report_date_ = today;

    auto report_gen = [this]() -> std::string {
        nlohmann::json report;
        report["generated_at"] = std::chrono::duration_cast<std::chrono::seconds>(
            Clock::now().time_since_epoch()).count();
        report["stats"]["frames_received"] = stats_.frames_received;
        report["stats"]["frames_sent"] = stats_.frames_sent;
        report["stats"]["bytes_received"] = stats_.bytes_received;
        report["stats"]["bytes_sent"] = stats_.bytes_sent;
        report["stats"]["uploads_completed"] = stats_.uploads_completed;
        report["stats"]["uploads_failed"] = stats_.uploads_failed;
        report["stats"]["motion_requests"] = stats_.motion_requests;
        report["stats"]["db_errors"] = stats_.db_errors;
        report["stats"]["protocol_errors"] = stats_.protocol_errors;
        report["robots_tracked"] = status_cache_->robotCount();
        return report.dump(2);
    };

    file_manager_->generateDailyReport(today, report_gen);
}

} // namespace mfms
