import XCTest
@testable import Aquavate

// Tests for the drink-sync wire format.
//
// These guard the failure that produced "Invalid drink data chunk": a chunk
// larger than the ATT payload (MTU-3) is truncated by the BLE stack, so its
// header claims more records than the bytes that arrive. The record grew from
// 10 to 14 bytes without the 20-records-per-chunk assumption being revisited.

final class BLEDrinkChunkTests: XCTestCase {

    // iOS negotiates an ATT MTU of 185; a notification carries MTU-3 bytes.
    private let iosNotificationPayload = 185 - 3

    // Build a chunk exactly as the firmware packs it: little-endian header
    // followed by fixed-size records.
    private func makeChunk(chunkIndex: UInt16, totalChunks: UInt16, recordCount: Int) -> Data {
        var data = Data()
        withUnsafeBytes(of: chunkIndex.littleEndian) { data.append(contentsOf: $0) }
        withUnsafeBytes(of: totalChunks.littleEndian) { data.append(contentsOf: $0) }
        data.append(UInt8(recordCount))
        data.append(0)  // reserved

        for i in 0..<recordCount {
            withUnsafeBytes(of: UInt32(i + 1).littleEndian) { data.append(contentsOf: $0) }        // record_id
            withUnsafeBytes(of: UInt32(1_700_000_000).littleEndian) { data.append(contentsOf: $0) } // timestamp
            withUnsafeBytes(of: Int16(250).littleEndian) { data.append(contentsOf: $0) }            // amount_ml
            withUnsafeBytes(of: UInt16(500).littleEndian) { data.append(contentsOf: $0) }           // bottle_level_ml
            data.append(0)  // type = gulp
            data.append(1)  // flags = synced
        }
        return data
    }

    // MARK: - Wire format

    func testRecordAndHeaderSizesMatchFirmware() {
        XCTAssertEqual(BLEDrinkRecord.size, 14)
        XCTAssertEqual(BLEDrinkDataChunk.headerSize, 6)
    }

    func testSafeChunkFitsIniOSNotificationPayload() {
        let bytes = BLEDrinkDataChunk.headerSize + BLEDrinkDataChunk.safeRecordsPerChunk * BLEDrinkRecord.size
        XCTAssertLessThanOrEqual(bytes, iosNotificationPayload,
                                 "Default chunk size must fit in one notification or it is truncated in transit")
    }

    // MARK: - Parsing

    func testParsesFullChunk() {
        let chunk = BLEDrinkDataChunk.parse(from: makeChunk(chunkIndex: 0, totalChunks: 3, recordCount: 12))

        XCTAssertEqual(chunk?.chunkIndex, 0)
        XCTAssertEqual(chunk?.totalChunks, 3)
        XCTAssertEqual(chunk?.recordCount, 12)
        XCTAssertEqual(chunk?.records.count, 12)
        XCTAssertEqual(chunk?.records.first?.recordId, 1)
        XCTAssertEqual(chunk?.records.first?.amountMl, 250)
        XCTAssertEqual(chunk?.records.last?.recordId, 12)
        XCTAssertEqual(chunk?.isLastChunk, false)
    }

    func testTruncatedChunkIsRejected() {
        // A 20-record chunk (286 bytes) cut down to what a 185-byte MTU allows:
        // the header still says 20 records, so the payload is short.
        let truncated = makeChunk(chunkIndex: 0, totalChunks: 1, recordCount: 20)
            .prefix(iosNotificationPayload)

        XCTAssertNil(BLEDrinkDataChunk.parse(from: Data(truncated)),
                     "A truncated chunk must not parse as valid records")
    }

    func testLastChunkDetection() {
        let last = BLEDrinkDataChunk.parse(from: makeChunk(chunkIndex: 2, totalChunks: 3, recordCount: 4))
        XCTAssertEqual(last?.isLastChunk, true)
    }

    func testEmptyInitialChunkParsesWithNoRecords() {
        // Firmware sets an empty value on init so subscribing does not deliver 0 bytes.
        let empty = BLEDrinkDataChunk.parse(from: makeChunk(chunkIndex: 0, totalChunks: 0, recordCount: 0))
        XCTAssertEqual(empty?.records.count, 0)
        XCTAssertEqual(empty?.totalChunks, 0)
    }
}
