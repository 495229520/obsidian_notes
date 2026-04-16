#include <gtest/gtest.h>
#include "mfms/protocol/messages.hpp"

using namespace mfms::protocol;

TEST(MessagesTest, UploadBeginSerializeDeserialize) {
    UploadBeginPayload orig;
    orig.upload_id = 123456789;
    orig.total_size = 1024 * 1024;  // 1MB
    orig.chunk_size = 65536;
    orig.file_crc32 = 0xDEADBEEF;
    orig.filename = "test_script.lua";
    orig.target_robot_id = "robot_001";

    std::vector<uint8_t> buf;
    ASSERT_TRUE(orig.serialize(buf));

    UploadBeginPayload parsed;
    ASSERT_TRUE(UploadBeginPayload::deserialize(buf.data(), buf.size(), parsed));

    EXPECT_EQ(parsed.upload_id, orig.upload_id);
    EXPECT_EQ(parsed.total_size, orig.total_size);
    EXPECT_EQ(parsed.chunk_size, orig.chunk_size);
    EXPECT_EQ(parsed.file_crc32, orig.file_crc32);
    EXPECT_EQ(parsed.filename, orig.filename);
    EXPECT_EQ(parsed.target_robot_id, orig.target_robot_id);
}

TEST(MessagesTest, UploadChunkSerializeDeserialize) {
    UploadChunkPayload orig;
    orig.upload_id = 999;
    orig.offset = 65536;
    orig.chunk_crc32 = 0x12345678;
    orig.data = {0x00, 0x01, 0x02, 0x03, 0xFF, 0xFE, 0xFD, 0xFC};

    std::vector<uint8_t> buf;
    ASSERT_TRUE(orig.serialize(buf));

    UploadChunkPayload parsed;
    ASSERT_TRUE(UploadChunkPayload::deserialize(buf.data(), buf.size(), parsed));

    EXPECT_EQ(parsed.upload_id, orig.upload_id);
    EXPECT_EQ(parsed.offset, orig.offset);
    EXPECT_EQ(parsed.chunk_crc32, orig.chunk_crc32);
    EXPECT_EQ(parsed.data, orig.data);
}

TEST(MessagesTest, UploadEndSerializeDeserialize) {
    UploadEndPayload orig;
    orig.upload_id = 42;
    orig.final_size = 500000;
    orig.final_crc32 = 0xABCDEF00;

    std::vector<uint8_t> buf;
    ASSERT_TRUE(orig.serialize(buf));

    UploadEndPayload parsed;
    ASSERT_TRUE(UploadEndPayload::deserialize(buf.data(), buf.size(), parsed));

    EXPECT_EQ(parsed.upload_id, orig.upload_id);
    EXPECT_EQ(parsed.final_size, orig.final_size);
    EXPECT_EQ(parsed.final_crc32, orig.final_crc32);
}

TEST(MessagesTest, MotionRequestSerializeDeserialize) {
    MotionRequestPayload orig;
    orig.robot_id = "agv_007";
    orig.command = R"({"action":"move","target":{"x":100,"y":200}})";
    orig.timeout_ms = 10000;

    std::vector<uint8_t> buf;
    ASSERT_TRUE(orig.serialize(buf));

    MotionRequestPayload parsed;
    ASSERT_TRUE(MotionRequestPayload::deserialize(buf.data(), buf.size(), parsed));

    EXPECT_EQ(parsed.robot_id, orig.robot_id);
    EXPECT_EQ(parsed.command, orig.command);
    EXPECT_EQ(parsed.timeout_ms, orig.timeout_ms);
}

TEST(MessagesTest, MotionResponseSerializeDeserialize) {
    MotionResponsePayload orig;
    orig.robot_id = "robot_123";
    orig.success = true;
    orig.error_code = 0;
    orig.result = R"({"status":"completed","position":{"x":100,"y":200}})";

    std::vector<uint8_t> buf;
    ASSERT_TRUE(orig.serialize(buf));

    MotionResponsePayload parsed;
    ASSERT_TRUE(MotionResponsePayload::deserialize(buf.data(), buf.size(), parsed));

    EXPECT_EQ(parsed.robot_id, orig.robot_id);
    EXPECT_EQ(parsed.success, orig.success);
    EXPECT_EQ(parsed.error_code, orig.error_code);
    EXPECT_EQ(parsed.result, orig.result);
}

TEST(MessagesTest, ErrorPayloadSerializeDeserialize) {
    ErrorPayload orig;
    orig.code = 1001;
    orig.detail = "Invalid frame format: magic bytes mismatch";

    std::vector<uint8_t> buf;
    ASSERT_TRUE(orig.serialize(buf));

    ErrorPayload parsed;
    ASSERT_TRUE(ErrorPayload::deserialize(buf.data(), buf.size(), parsed));

    EXPECT_EQ(parsed.code, orig.code);
    EXPECT_EQ(parsed.detail, orig.detail);
}

TEST(MessagesTest, AckPayloadSerializeDeserialize) {
    AckPayload orig;
    orig.status = 0;
    orig.detail = "Upload chunk received";

    std::vector<uint8_t> buf;
    ASSERT_TRUE(orig.serialize(buf));

    AckPayload parsed;
    ASSERT_TRUE(AckPayload::deserialize(buf.data(), buf.size(), parsed));

    EXPECT_EQ(parsed.status, orig.status);
    EXPECT_EQ(parsed.detail, orig.detail);
}

TEST(MessagesTest, ServerLogRequestSerializeDeserialize) {
    ServerLogRequestPayload orig;
    orig.date = "2024-01-15";
    orig.offset = 1000;
    orig.max_size = 65536;

    std::vector<uint8_t> buf;
    ASSERT_TRUE(orig.serialize(buf));

    ServerLogRequestPayload parsed;
    ASSERT_TRUE(ServerLogRequestPayload::deserialize(buf.data(), buf.size(), parsed));

    EXPECT_EQ(parsed.date, orig.date);
    EXPECT_EQ(parsed.offset, orig.offset);
    EXPECT_EQ(parsed.max_size, orig.max_size);
}

TEST(MessagesTest, SanitizeFilenameValid) {
    std::string output;

    EXPECT_TRUE(sanitizeFilename("script.lua", output));
    EXPECT_EQ(output, "script.lua");

    EXPECT_TRUE(sanitizeFilename("my_file-2024.log", output));
    EXPECT_EQ(output, "my_file-2024.log");

    EXPECT_TRUE(sanitizeFilename("test123", output));
    EXPECT_EQ(output, "test123");
}

TEST(MessagesTest, SanitizeFilenameInvalid) {
    std::string output;

    // Path traversal
    EXPECT_FALSE(sanitizeFilename("../etc/passwd", output));
    EXPECT_FALSE(sanitizeFilename("/etc/passwd", output));
    EXPECT_FALSE(sanitizeFilename("..\\windows\\system32", output));

    // Hidden files
    EXPECT_FALSE(sanitizeFilename(".hidden", output));

    // Empty
    EXPECT_FALSE(sanitizeFilename("", output));

    // Special characters
    EXPECT_FALSE(sanitizeFilename("file:name", output));
    EXPECT_FALSE(sanitizeFilename("file*name", output));
    EXPECT_FALSE(sanitizeFilename("file?name", output));
}

TEST(MessagesTest, SanitizeFilenameNormalize) {
    std::string output;

    // Replace spaces with underscore
    EXPECT_TRUE(sanitizeFilename("my file.txt", output));
    EXPECT_EQ(output, "my_file.txt");

    // Replace special chars with underscore
    EXPECT_TRUE(sanitizeFilename("file@name.txt", output));
    EXPECT_EQ(output, "file_name.txt");
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
