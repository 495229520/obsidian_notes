#include <gtest/gtest.h>
#include "mfms/protocol/frame.hpp"
#include "mfms/protocol/frame_parser.hpp"

using namespace mfms::protocol;

TEST(FrameTest, SerializeDeserializeHeader) {
    FrameHeader hdr;
    hdr.version = kProtocolVersion;
    hdr.msg_type = MsgType::STATUS_SNAPSHOT_REQ;
    hdr.flags = FrameFlags::REQUIRES_ACK;
    hdr.request_id = 12345;
    hdr.payload_len = 100;

    uint8_t buf[kHeaderSize];
    serializeHeader(buf, hdr);

    auto parsed = deserializeHeader(buf);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->version, hdr.version);
    EXPECT_EQ(parsed->msg_type, hdr.msg_type);
    EXPECT_EQ(static_cast<uint32_t>(parsed->flags), static_cast<uint32_t>(hdr.flags));
    EXPECT_EQ(parsed->request_id, hdr.request_id);
    EXPECT_EQ(parsed->payload_len, hdr.payload_len);
}

TEST(FrameTest, InvalidMagic) {
    uint8_t buf[kHeaderSize] = {0};
    buf[0] = 'X';  // Invalid magic
    auto parsed = deserializeHeader(buf);
    EXPECT_FALSE(parsed.has_value());
}

TEST(FrameTest, UnsupportedVersion) {
    FrameHeader hdr;
    hdr.version = 99;  // Unsupported
    hdr.msg_type = MsgType::ACK;

    uint8_t buf[kHeaderSize];
    serializeHeader(buf, hdr);
    // Fix magic but keep wrong version
    buf[4] = 99;
    buf[5] = 0;

    auto parsed = deserializeHeader(buf);
    EXPECT_FALSE(parsed.has_value());
}

TEST(FrameTest, PayloadTooLarge) {
    FrameHeader hdr;
    hdr.payload_len = kMaxPayloadLen + 1;  // Too large

    uint8_t buf[kHeaderSize];
    serializeHeader(buf, hdr);

    auto parsed = deserializeHeader(buf);
    EXPECT_FALSE(parsed.has_value());
}

TEST(FrameTest, CRC32) {
    uint8_t data[] = "Hello, World!";
    uint32_t crc = crc32(data, sizeof(data) - 1);
    EXPECT_NE(crc, 0u);

    // Same data should produce same CRC
    uint32_t crc2 = crc32(data, sizeof(data) - 1);
    EXPECT_EQ(crc, crc2);

    // Different data should produce different CRC
    data[0] = 'h';
    uint32_t crc3 = crc32(data, sizeof(data) - 1);
    EXPECT_NE(crc, crc3);
}

TEST(FrameTest, HeaderCrcValidation) {
    FrameHeader hdr;
    hdr.msg_type = MsgType::MOTION_REQ;
    hdr.flags = FrameFlags::HAS_CRC;
    hdr.request_id = 42;
    hdr.payload_len = 50;

    uint8_t buf[kHeaderSize];
    serializeHeader(buf, hdr);

    auto parsed = deserializeHeader(buf);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_TRUE(validateHeaderCrc(buf, *parsed));

    // Corrupt the header
    buf[12] ^= 0xFF;
    parsed = deserializeHeader(buf);
    if (parsed) {
        EXPECT_FALSE(validateHeaderCrc(buf, *parsed));
    }
}

TEST(FrameParserTest, SingleFrame) {
    FrameParser parser;

    FrameHeader hdr;
    hdr.msg_type = MsgType::ACK;
    hdr.request_id = 1;
    hdr.payload_len = 4;

    uint8_t frame[kHeaderSize + 4];
    serializeHeader(frame, hdr);
    frame[kHeaderSize] = 'T';
    frame[kHeaderSize + 1] = 'E';
    frame[kHeaderSize + 2] = 'S';
    frame[kHeaderSize + 3] = 'T';

    size_t consumed = parser.feed(frame, sizeof(frame));
    EXPECT_EQ(consumed, sizeof(frame));
    EXPECT_TRUE(parser.hasFrame());
    EXPECT_FALSE(parser.hasError());

    EXPECT_EQ(parser.header().msg_type, MsgType::ACK);
    EXPECT_EQ(parser.header().request_id, 1u);
    EXPECT_EQ(parser.payload().size(), 4u);
    EXPECT_EQ(parser.payload()[0], 'T');
}

TEST(FrameParserTest, PartialHeader) {
    FrameParser parser;

    FrameHeader hdr;
    hdr.msg_type = MsgType::ERROR;
    hdr.payload_len = 0;

    uint8_t header_buf[kHeaderSize];
    serializeHeader(header_buf, hdr);

    // Feed first half
    size_t consumed = parser.feed(header_buf, kHeaderSize / 2);
    EXPECT_EQ(consumed, kHeaderSize / 2);
    EXPECT_FALSE(parser.hasFrame());
    EXPECT_FALSE(parser.hasError());

    // Feed second half
    consumed = parser.feed(header_buf + kHeaderSize / 2, kHeaderSize / 2);
    EXPECT_EQ(consumed, kHeaderSize / 2);
    EXPECT_TRUE(parser.hasFrame());
}

TEST(FrameParserTest, PartialPayload) {
    FrameParser parser;

    FrameHeader hdr;
    hdr.msg_type = MsgType::STATUS_SNAPSHOT_RESP;
    hdr.payload_len = 100;

    uint8_t header_buf[kHeaderSize];
    serializeHeader(header_buf, hdr);

    parser.feed(header_buf, kHeaderSize);
    EXPECT_FALSE(parser.hasFrame());

    // Feed payload in chunks
    std::vector<uint8_t> payload(100, 0xAB);
    parser.feed(payload.data(), 50);
    EXPECT_FALSE(parser.hasFrame());

    parser.feed(payload.data() + 50, 50);
    EXPECT_TRUE(parser.hasFrame());
    EXPECT_EQ(parser.payload().size(), 100u);
}

TEST(FrameParserTest, MultipleFrames) {
    FrameParser parser;

    // Create two frames back to back
    FrameHeader hdr1, hdr2;
    hdr1.msg_type = MsgType::ACK;
    hdr1.request_id = 1;
    hdr1.payload_len = 2;

    hdr2.msg_type = MsgType::ERROR;
    hdr2.request_id = 2;
    hdr2.payload_len = 3;

    std::vector<uint8_t> data;
    data.resize(kHeaderSize * 2 + 5);

    serializeHeader(data.data(), hdr1);
    data[kHeaderSize] = 'A';
    data[kHeaderSize + 1] = 'B';

    serializeHeader(data.data() + kHeaderSize + 2, hdr2);
    data[kHeaderSize * 2 + 2] = 'X';
    data[kHeaderSize * 2 + 3] = 'Y';
    data[kHeaderSize * 2 + 4] = 'Z';

    // Parse first frame
    size_t consumed = parser.feed(data.data(), data.size());
    EXPECT_TRUE(parser.hasFrame());
    EXPECT_EQ(parser.header().request_id, 1u);
    EXPECT_EQ(consumed, kHeaderSize + 2);

    // Reset and parse second frame
    parser.reset();
    consumed = parser.feed(data.data() + kHeaderSize + 2, data.size() - kHeaderSize - 2);
    EXPECT_TRUE(parser.hasFrame());
    EXPECT_EQ(parser.header().request_id, 2u);
}

TEST(FrameParserTest, OversizedPayload) {
    FrameParser parser(1000);  // Max 1000 bytes

    FrameHeader hdr;
    hdr.msg_type = MsgType::STATUS_SNAPSHOT_RESP;
    hdr.payload_len = 2000;  // Over limit

    uint8_t header_buf[kHeaderSize];
    serializeHeader(header_buf, hdr);

    parser.feed(header_buf, kHeaderSize);
    EXPECT_TRUE(parser.hasError());
    EXPECT_EQ(parser.lastError(), mfms::ErrorCode::PAYLOAD_TOO_LARGE);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
