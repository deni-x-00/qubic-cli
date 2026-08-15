#include <cinttypes>
#include <cstring>
#include <cstdio>
#include <ctime>

#include "stdint.h"
#include "quottery.h"
#include "prompt.h"
#include "structs.h"
#include "key_utils.h"
#include "node_utils.h"
#include "k12_and_key_utils.h"
#include "connection.h"
#include "logger.h"
#include "wallet_utils.h"
constexpr int QUOTTERY_CONTRACT_ID = 2;
/**
 * @return pack DateAndTime data from year, month, day, hour, minute, second, millisec, microsec to a uint64_t
 * Bit layout: year(18) | month(4) | day(5) | hour(5) | minute(6) | second(6) | millisec(10) | microsec(10)
 */
static void packDateTime(uint32_t _year, uint32_t _month, uint32_t _day, uint32_t _hour, uint32_t _minute, uint32_t _second, uint32_t _millisec, uint32_t _microsec, uint64_t& res)
{
    res = ((uint64_t)_year << 46) | ((uint64_t)_month << 42) | ((uint64_t)_day << 37) | ((uint64_t)_hour << 32)
        | ((uint64_t)_minute << 26) | ((uint64_t)_second << 20) | ((uint64_t)_millisec << 10) | (uint64_t)_microsec;
}

#define DATETIME_GET_YEAR(data) ((data >> 46))
#define DATETIME_GET_MONTH(data) ((data >> 42) & 0b1111)
#define DATETIME_GET_DAY(data) ((data >> 37) & 0b11111)
#define DATETIME_GET_HOUR(data) ((data >> 32) & 0b11111)
#define DATETIME_GET_MINUTE(data) ((data >> 26) & 0b111111)
#define DATETIME_GET_SECOND(data) ((data >> 20) & 0b111111)
#define DATETIME_GET_MILLISEC(data) ((data >> 10) & 0b1111111111)
#define DATETIME_GET_MICROSEC(data) ((data) & 0b1111111111)

/**
* @return unpack DateAndTime from uint64 to year, month, day, hour, minute, second, millisec, microsec
*/
void unpackDateTime(uint32_t& _year, uint8_t& _month, uint8_t& _day, uint8_t& _hour, uint8_t& _minute, uint8_t& _second, uint16_t& _millisec, uint16_t& _microsec, uint64_t data)
{
    _year = DATETIME_GET_YEAR(data); // 18 bits
    _month = DATETIME_GET_MONTH(data); // 4 bits
    _day = DATETIME_GET_DAY(data); // 5 bits
    _hour = DATETIME_GET_HOUR(data); // 5 bits
    _minute = DATETIME_GET_MINUTE(data); // 6 bits
    _second = DATETIME_GET_SECOND(data); // 6 bits
    _millisec = DATETIME_GET_MILLISEC(data); // 10 bits
    _microsec = DATETIME_GET_MICROSEC(data); // 10 bits
}

// QTRY PROCEDURES
#define QTRY_CREATE_EVENT 1
#define QTRY_ADD_ASK_ORDER    2
#define QTRY_REMOVE_ASK_ORDER 3
#define QTRY_ADD_BID_ORDER    4
#define QTRY_REMOVE_BID_ORDER 5
#define QTRY_PUBLISH_RESULT 6
#define QTRY_TRY_FINALIZE_EVENT 7
#define QTRY_DISPUTE 8
#define QTRY_RESOLVE_DISPUTE 9
#define QTRY_USER_CLAIM_REWARD 10
#define QTRY_GO_FORCE_CLAIM_REWARD 11
#define QTRY_TRANSFER_QUSD 12
#define QTRY_TRANSFER_SHARE_MANAGEMENT_RIGHTS 13
#define QTRY_CLEAN_MEMORY 14
#define QTRY_TRANSFER_QTRYGOV 15
#define QTRY_UPDATE_FEE_DISCOUNT_LIST 20
#define QTRY_CREATE_EVENT_GROUP 30
#define QTRY_ADD_MARKET 31
#define QTRY_OPEN_EVENT 32
#define QTRY_PUBLISH_EVENT_RESULT 33
#define QTRY_DISPUTE_EVENT_RESULT 34
#define QTRY_RESOLVE_EVENT_DISPUTE 35
#define QTRY_CANCEL_EVENT_GROUP 36
#define QTRY_PROPOSAL_VOTE 100
// QTRY FUNCTIONS
#define QTRY_GET_BASIC 1
#define QTRY_GET_EVENT 2
#define QTRY_GET_ORDERS 3
#define QTRY_GET_ACTIVE_EVENTS 4
#define QTRY_GET_EVENT_BATCH 5
#define QUOTTERY_GET_USER_POSITION 6
#define QTRY_GET_APPROVED_AMOUNT 7
#define QTRY_GET_TOP_PROPOSALS 8
#define QTRY_GET_EVENT_GROUP 9
#define QTRY_GET_MARKET_EVENT_GROUP 10
#define QTRY_GET_EVENT_GROUP_INFO_BATCH 11

#define QUOTTERY_EO_GET_OPTION(eo)  ((eo) >> 63)
#define QUOTTERY_EO_GET_EVENTID(eo) ((eo) & 0x3FFFFFFFFFFFFFFFULL)

static int64_t getBalanceNumber(QCPtr& qc, const uint8_t* publicKey) {
    struct {
        RequestResponseHeader header;
        RequestedEntity req;
    } packet;
    packet.header.setSize(sizeof(packet));
    packet.header.randomizeDejavu();
    packet.header.setType(REQUEST_ENTITY);
    memcpy(packet.req.publicKey, publicKey, 32);
    qc->sendData(packet);
    auto result = qc->receivePacketWithHeaderAs<RespondedEntity>();
    return result.entity.incomingAmount - result.entity.outgoingAmount;
}

void quotteryGetBasicInfo(QCPtr& qc, qtryBasicInfo_output& result)
{
    memset(&result, 0, sizeof(result));
    // Note: the QCPtr overload is preserved so we can pass an already-open connection.
    // We reuse runContractFunction by extracting nodeIp/nodePort would not be possible here;
    // for callers that have only nodeIp/nodePort, prefer quotteryGetBasicInfoByIp().
    struct {
        RequestResponseHeader header;
        RequestContractFunction rcf;
    } packet;
    packet.header.setSize(sizeof(packet));
    packet.header.randomizeDejavu();
    packet.header.setType(RequestContractFunction::type());
    packet.rcf.inputSize = 0;
    packet.rcf.inputType = QTRY_GET_BASIC;
    packet.rcf.contractIndex = QUOTTERY_CONTRACT_ID;
    qc->sendData(packet);

    try
    {
        result = qc->receivePacketWithHeaderAs<qtryBasicInfo_output>();
    }
    catch (std::logic_error)
    {
        memset(&result, 0, sizeof(qtryBasicInfo_output));
    }
}

void quotteryPrintBasicInfo(const char* nodeIp, const int nodePort)
{
    qtryBasicInfo_output result{};
    if (!runContractFunction(nodeIp, nodePort, QUOTTERY_CONTRACT_ID, QTRY_GET_BASIC, nullptr, 0, &result, sizeof(result)))
    {
        LOG("Failed to get basic info\n");
        return;
    }
    LOG("Operation Fee: %.2f%%\n", result.operationFee / 10.0);
    LOG("Shareholders fee: %.2f%%\n", result.shareholderFee / 10.0);
    LOG("Burn fee: %.2f%%\n", result.burnFee / 10.0);
    LOG("================\n");
    LOG("Number of issued events: %" PRIu64 "\n", result.nIssuedEvent);
    LOG("Number of issued event groups: %" PRIu64 "\n", result.nIssuedEventGroup);
    LOG("Shareholders revenue: %" PRIu64 "\n", result.shareholdersRevenue);
    LOG("Operation revenue: %" PRIu64 "\n", result.operationRevenue);
    LOG("Burned amount: %" PRIu64 "\n", result.burnedAmount);
    LOG("feePerDay: %" PRIu64 "\n", result.feePerDay);
    LOG("antiSpamAmount: %" PRIu64 "\n", result.antiSpamAmount);
    LOG("depositAmountForDispute: %" PRIu64 "\n", result.depositAmountForDispute);
    char buf[64] = { 0 };
    getIdentityFromPublicKey(result.gameOperator, buf, false);
    LOG("Game operator ID: %s\n", buf);
}

void quotteryGetActiveEvents(const char* nodeIp, int nodePort, getActiveEvent_output& result)
{
    memset(&result, 0, sizeof(result));
    if (!runContractFunction(nodeIp, nodePort, QUOTTERY_CONTRACT_ID, QTRY_GET_ACTIVE_EVENTS, nullptr, 0, &result, sizeof(result)))
    {
        memset(&result, 0, sizeof(result));
    }
}

void quotteryPrintActiveEvents(const char* nodeIp, int nodePort)
{
    getActiveEvent_output result{};
    quotteryGetActiveEvents(nodeIp, nodePort, result);

    if (isArrayZero(reinterpret_cast<uint8_t*>(&result), sizeof(result)))
    {
        LOG("Failed to get recent active events\n");
        return;
    }

    LOG("Recent active event IDs:\n");
    bool hasAny = false;
    for (size_t i = 0; i < QUOTTERY_MAX_CONCURRENT_EVENT; ++i)
    {
        if (result.recentActiveEvent[i] == uint64_t(-1))
            continue;

        LOG("%" PRIu64 "\n", result.recentActiveEvent[i]);
        hasAny = true;
    }

    if (!hasAny)
    {
        LOG("(none)\n");
    }
}

#define QTRY_GET_YEAR(data) ((data >> 26)+24)
#define QTRY_GET_MONTH(data) ((data >> 22) & 0b1111)
#define QTRY_GET_DAY(data) ((data >> 17) & 0b11111)
#define QTRY_GET_HOUR(data) ((data >> 12) & 0b11111)
#define QTRY_GET_MINUTE(data) ((data >> 6) & 0b111111)
#define QTRY_GET_SECOND(data) ((data) & 0b111111)

/**
* @return unpack qtry datetime from uin32 to year, month, day, hour, minute, secon
*/
void unpackQuotteryDate(uint8_t& _year, uint8_t& _month, uint8_t& _day, uint8_t& _hour, uint8_t& _minute, uint8_t& _second, uint32_t data)
{
    _year = QTRY_GET_YEAR(data); // 6 bits
    _month = QTRY_GET_MONTH(data); //4bits
    _day = QTRY_GET_DAY(data); //5bits
    _hour = QTRY_GET_HOUR(data); //5bits
    _minute = QTRY_GET_MINUTE(data); //6bits
    _second = QTRY_GET_SECOND(data); //6bits
}


struct QuotteryCreateEvent_input
{
    uint64_t eid;
    uint64_t openDate; // submitted date
    uint64_t endDate; // stop receiving result from OPs
    uint8_t desc[128];
    uint8_t option0Desc[64];
    uint8_t option1Desc[64];
};

void quotteryCreateEvent(const char* nodeIp, int nodePort, const char* seed,
    const std::string eventDesc,
    const std::string opt0Desc,
    const std::string opt1Desc,
    const std::string endDate,
    uint16_t tagId,
    uint32_t scheduledTickOffset)
{
    QuotteryCreateEvent_input cei{};

    // Copy desc text, but cap at 128 bytes; pack tagId as uint16 LE into desc[126:128]
    memcpy(cei.desc, eventDesc.c_str(), std::min(int(eventDesc.size()), 128));
    cei.desc[126] = (uint8_t)(tagId & 0xFF);
    cei.desc[127] = (uint8_t)((tagId >> 8) & 0xFF);
    memcpy(cei.option0Desc, opt0Desc.c_str(), std::min(int(opt0Desc.size()), 64));
    memcpy(cei.option1Desc, opt1Desc.c_str(), std::min(int(opt1Desc.size()), 64));

    {
        auto buff = endDate.data();
        if (strlen(buff) != 19 || buff[4] != '-' || buff[7] != '-' || buff[10] != ' ' || buff[13] != ':' ||
            buff[16] != ':' ||
            !isdigit(buff[0]) || !isdigit(buff[1]) || !isdigit(buff[2]) || !isdigit(buff[3]) ||
            !isdigit(buff[5]) || !isdigit(buff[6]) || !isdigit(buff[8]) || !isdigit(buff[9]) ||
            !isdigit(buff[11]) || !isdigit(buff[12]) || !isdigit(buff[14]) || !isdigit(buff[15]) ||
            !isdigit(buff[17]) || !isdigit(buff[18])) {
            LOG("Error: Invalid date-time format. Please follow the format: YYYY-MM-DD hh:mm:ss\n");
            exit(EXIT_FAILURE);
        }

        uint32_t year = (buff[0] - 48) * 1000 + (buff[1] - 48) * 100 + (buff[2] - 48) * 10 + (buff[3] - 48);
        uint8_t month = (buff[5] - 48) * 10 + (buff[6] - 48);
        uint8_t day = (buff[8] - 48) * 10 + (buff[9] - 48);
        uint8_t hour = (buff[11] - 48) * 10 + (buff[12] - 48);
        uint8_t minute = (buff[14] - 48) * 10 + (buff[15] - 48);
        uint8_t sec = (buff[17] - 48) * 10 + (buff[18] - 48);
        packDateTime(year, month, day, hour, minute, sec, 0, 0, cei.endDate);
    }

    // eid and openDate are set by the SC
    cei.eid = 0;
    cei.openDate = 0;

    LOG("Crafting transaction...\n");
    LOG("Tag ID: %u\n", tagId);
    
    makeContractTransaction(nodeIp, nodePort, seed,
        QUOTTERY_CONTRACT_ID,
        QTRY_CREATE_EVENT,
        /*amount=*/0,
        sizeof(cei), &cei,
        scheduledTickOffset);
}

void _quotteryGetEventInfo(QCPtr& qc, uint64_t eventId, getEventInfo_output& result)
{
    // Kept for callers that already hold a QCPtr.
    struct {
        RequestResponseHeader header;
        RequestContractFunction rcf;
        getEventInfo_input input;
    } packet;
    packet.header.setSize(sizeof(packet));
    packet.header.randomizeDejavu();
    packet.header.setType(RequestContractFunction::type());
    packet.rcf.inputSize = sizeof(getEventInfo_input);
    packet.rcf.inputType = QTRY_GET_EVENT;
    packet.rcf.contractIndex = QUOTTERY_CONTRACT_ID;
    packet.input.eventId = eventId;
    qc->sendData(packet);

    try
    {
        result = qc->receivePacketWithHeaderAs<getEventInfo_output>();
    }
    catch (std::logic_error)
    {
        memset(&result, 0, sizeof(getEventInfo_output));
        result.resultByGO = -1;
    }
}

void quotteryGetEventInfo(const char* nodeIp, const int nodePort, uint64_t eventId, getEventInfo_output& result)
{
    getEventInfo_input input{};
    input.eventId = eventId;
    memset(&result, 0, sizeof(result));
    if (!runContractFunction(nodeIp, nodePort, QUOTTERY_CONTRACT_ID, QTRY_GET_EVENT,
                             &input, sizeof(input), &result, sizeof(result)))
    {
        memset(&result, 0, sizeof(result));
        result.resultByGO = -1;
    }
}

static void quotteryPrintEventMetaData(const QtryEventInfo& result, uint64_t requestedEventId)
{
    if (result.eid != requestedEventId)
    {
        LOG("EventId #%" PRIu64 " doesn't exist\n", requestedEventId);
        return;
    }

    char buf[128] = { 0 };
    LOG("Event Id: %" PRIu64 "\n", result.eid);
    {
        memset(buf, 0, sizeof(buf));
        memcpy(buf, result.desc, sizeof(result.desc));
        LOG("Event description: %s\n", buf);
    }
    {
        memset(buf, 0, sizeof(buf));
        memcpy(buf, result.option0Desc, sizeof(result.option0Desc));
        LOG("Option 0: %s\n", buf);
    }
    {
        memset(buf, 0, sizeof(buf));
        memcpy(buf, result.option1Desc, sizeof(result.option1Desc));
        LOG("Option 1: %s\n", buf);
    }
    {
        uint32_t year;
        uint8_t month, day, hour, minute, second;
        uint16_t _millisec;
        uint16_t _microsec;
        unpackDateTime(year, month, day, hour, minute, second, _millisec, _microsec, result.openDate);
        LOG("Open date: %04u-%02u-%02u %02u:%02u:%02u\n", year, month, day, hour, minute, second);

        unpackDateTime(year, month, day, hour, minute, second, _millisec, _microsec, result.endDate);
        LOG("End date:   %04u-%02u-%02u %02u:%02u:%02u\n", year, month, day, hour, minute, second);
    }
}

static void quotteryPrintEventInfoRecord(const getEventInfo_output& result, uint64_t requestedEventId)
{
    if (result.qei.eid != requestedEventId)
    {
        LOG("EventId #%" PRIu64 " doesn't exist\n", requestedEventId);
        return;
    }
    quotteryPrintEventMetaData(result.qei, requestedEventId);

    LOG("Result by GO: %" PRId32 "\n", result.resultByGO);
    if (result.resultByGO != -1)
    {
        if (result.publishTickTime == 0xffffffffu) {
            LOG("This event is already finalized and waiting for cleanup\n");
        }
        else {
            LOG("Publish tick time: %" PRIu32 "\n", result.publishTickTime);
        }
    }

    if (!isZeroPubkey(result.disputerInfo.pubkey))
    {
        char disputerId[128] = { 0 };
        getIdentityFromPublicKey(result.disputerInfo.pubkey, disputerId, false);
        LOG("Disputer: %s\n", disputerId);
        LOG("Dispute amount: %" PRId64 "\n", result.disputerInfo.amount);
        LOG("Computors vote 0: %" PRIu32 "\n", result.computorsVote0);
        LOG("Computors vote 1: %" PRIu32 "\n", result.computorsVote1);
    }
}

void quotteryPrintEventInfo(const char* nodeIp, const int nodePort, uint64_t eventId)
{
    getEventInfo_output result;
    memset(&result, 0, sizeof(getEventInfo_output));
    LOG("Getting eventId #%" PRIu64 " info...\n", eventId);
    quotteryGetEventInfo(nodeIp, nodePort, eventId, result);
    if (isArrayZero((uint8_t*)&result, sizeof(getEventInfo_output)))
    {
        LOG("Failed to get\n");
        return;
    }

    quotteryPrintEventInfoRecord(result, eventId);
}

void quotteryGetEventInfoBatch(const char* nodeIp, int nodePort, const uint64_t* eventIds, GetEventInfoBatch_output& result)
{
    GetEventInfoBatch_input input{};
    for (size_t j = 0; j < 64; ++j)
    {
        input.eventIds[j] = eventIds[j];
    }

    memset(&result, 0, sizeof(result));
    if (!runContractFunction(nodeIp, nodePort, QUOTTERY_CONTRACT_ID, QTRY_GET_EVENT_BATCH,
                             &input, sizeof(input), &result, sizeof(result)))
    {
        memset(&result, 0, sizeof(result));
    }
}

void quotteryPrintEventInfoBatch(const char* nodeIp, int nodePort, const uint64_t* eventIds, size_t count)
{
    if (count == 0)
    {
        LOG("Error: no event ids provided\n");
        return;
    }

    uint64_t paddedEventIds[64] = {};
    for (size_t i = 0; i < count && i < 64; ++i)
    {
        paddedEventIds[i] = eventIds[i];
    }

    GetEventInfoBatch_output result{};
    memset(&result, 2, sizeof(GetEventInfoBatch_output));
    LOG("Getting %zu event(s) info in batch...\n", count);
    quotteryGetEventInfoBatch(nodeIp, nodePort, paddedEventIds, result);

    if (isArrayZero(reinterpret_cast<uint8_t*>(&result), sizeof(result)))
    {
        LOG("Failed to get batch event info\n");
        return;
    }

    for (size_t i = 0; i < count && i < 64; ++i)
    {
        LOG("\n================\n");
        LOG("Requested eventId: %" PRIu64 "\n", paddedEventIds[i]);
        quotteryPrintEventMetaData(result.aqei[i], paddedEventIds[i]);
    }
}

static const char* quotteryEventGroupModeName(uint8_t mode)
{
    switch (mode)
    {
    case QUOTTERY_EVENT_GROUP_MODE_INDEPENDENT: return "INDEPENDENT";
    case QUOTTERY_EVENT_GROUP_MODE_EXCLUSIVE_ONE: return "EXCLUSIVE_ONE";
    default: return "UNKNOWN";
    }
}

static const char* quotteryEventGroupStatusName(uint8_t status)
{
    switch (status)
    {
    case QUOTTERY_EVENT_GROUP_STATUS_DRAFT: return "DRAFT";
    case QUOTTERY_EVENT_GROUP_STATUS_OPEN: return "OPEN";
    case QUOTTERY_EVENT_GROUP_STATUS_RESOLVING: return "RESOLVING";
    case QUOTTERY_EVENT_GROUP_STATUS_FINALIZED: return "FINALIZED";
    default: return "UNKNOWN";
    }
}

static void quotteryPrintPackedDateTime(const char* label, uint64_t value)
{
    if (value == 0)
    {
        LOG("%s: (not set)\n", label);
        return;
    }

    uint32_t year = 0;
    uint8_t month = 0, day = 0, hour = 0, minute = 0, second = 0;
    uint16_t millisec = 0, microsec = 0;
    unpackDateTime(year, month, day, hour, minute, second, millisec, microsec, value);
    LOG("%s: %04u-%02u-%02u %02u:%02u:%02u UTC\n",
        label, year, month, day, hour, minute, second);
}

static void quotteryPrintEventGroupInfoRecord(
    const QtryEventGroupInfo& info,
    int32_t winningMarketIndex,
    bool exists)
{
    if (!exists)
    {
        LOG("Event group does not exist\n");
        return;
    }

    char description[129] = {};
    memcpy(description, info.desc, sizeof(info.desc));

    LOG("Event group ID: %" PRIu64 "\n", info.eventGroupId);
    LOG("Description: %s\n", description);
    LOG("Mode: %s (%u)\n", quotteryEventGroupModeName(info.mode), info.mode);
    LOG("Status: %s (%u)\n", quotteryEventGroupStatusName(info.status), info.status);
    LOG("Markets: %u / %u\n", info.marketCount, info.expectedMarketCount);
    LOG("Finalized markets: %u\n", info.finalizedMarketCount);
    LOG("Archived markets: %u\n", info.archivedMarketCount);
    LOG("Winning market index: %" PRId32 "\n", winningMarketIndex);
    quotteryPrintPackedDateTime("Created date", info.createdDate);
    quotteryPrintPackedDateTime("Opened date", info.openedDate);
}

void quotteryPrintEventGroup(const char* nodeIp, int nodePort, uint64_t eventGroupId)
{
    GetEventGroup_input input{};
    input.eventGroupId = eventGroupId;
    GetEventGroup_output result{};

    if (!runContractFunction(nodeIp, nodePort, QUOTTERY_CONTRACT_ID, QTRY_GET_EVENT_GROUP,
                             &input, sizeof(input), &result, sizeof(result)))
    {
        LOG("Failed to get event group %" PRIu64 "\n", eventGroupId);
        return;
    }

    if (!result.exists)
    {
        LOG("Event group %" PRIu64 " does not exist\n", eventGroupId);
        return;
    }

    quotteryPrintEventGroupInfoRecord(result.eventGroupInfo, result.winningMarketIndex, true);
    if (result.winningMarketIndex >= 0)
    {
        LOG("Winning market ID: %" PRIu64 "\n", result.winningMarketId);
        LOG("Publish tick: %" PRIu32 "\n", result.publishTickTime);
    }

    if (!isZeroPubkey(result.disputerInfo.pubkey))
    {
        char disputerId[128] = {};
        getIdentityFromPublicKey(result.disputerInfo.pubkey, disputerId, false);
        LOG("Disputer: %s\n", disputerId);
        LOG("Dispute amount: %" PRId64 "\n", result.disputerInfo.amount);
    }

    LOG("Market IDs:\n");
    const size_t marketCount = std::min<size_t>(
        result.eventGroupInfo.marketCount,
        QUOTTERY_MAX_MARKETS_PER_EVENT_GROUP);
    for (size_t i = 0; i < marketCount; ++i)
    {
        LOG("  [%zu] %" PRIu64 "\n", i, result.markets.marketIds[i]);
    }
    if (marketCount == 0)
    {
        LOG("  (none)\n");
    }
}

void quotteryPrintMarketEventGroup(const char* nodeIp, int nodePort, uint64_t marketId)
{
    GetMarketEventGroup_input input{};
    input.marketId = marketId;
    GetMarketEventGroup_output result{};

    if (!runContractFunction(nodeIp, nodePort, QUOTTERY_CONTRACT_ID, QTRY_GET_MARKET_EVENT_GROUP,
                             &input, sizeof(input), &result, sizeof(result)))
    {
        LOG("Failed to get event group for market %" PRIu64 "\n", marketId);
        return;
    }

    if (!result.exists)
    {
        LOG("Market %" PRIu64 " is not linked to an event group\n", marketId);
        return;
    }

    LOG("Market ID: %" PRIu64 "\n", marketId);
    LOG("Event group ID: %" PRIu64 "\n", result.marketGroupLink.eventGroupId);
    LOG("Market index: %u\n", result.marketGroupLink.marketIndex);
    LOG("Group mode: %s (%u)\n", quotteryEventGroupModeName(result.mode), result.mode);
    LOG("Group status: %s (%u)\n", quotteryEventGroupStatusName(result.status), result.status);
}

void quotteryPrintEventGroupInfoBatch(
    const char* nodeIp,
    int nodePort,
    const uint64_t* eventGroupIds,
    size_t count)
{
    GetEventGroupInfoBatch_input input{};
    for (size_t i = 0; i < count && i < 64; ++i)
    {
        input.eventGroupIds[i] = eventGroupIds[i];
    }

    GetEventGroupInfoBatch_output result{};
    if (!runContractFunction(nodeIp, nodePort, QUOTTERY_CONTRACT_ID, QTRY_GET_EVENT_GROUP_INFO_BATCH,
                             &input, sizeof(input), &result, sizeof(result)))
    {
        LOG("Failed to get event group info batch\n");
        return;
    }

    for (size_t i = 0; i < count && i < 64; ++i)
    {
        LOG("\n================\n");
        LOG("Requested event group ID: %" PRIu64 "\n", eventGroupIds[i]);
        quotteryPrintEventGroupInfoRecord(
            result.eventGroupInfos[i],
            result.winningMarketIndices[i],
            result.exists[i] != 0);
    }
}

struct qtryOrderAction_input
{
    uint64_t eventId;
    uint64_t option;
    uint64_t amount;
    uint64_t price;
};
template <int functionNumber>
void qtryOrderAction(const char* nodeIp, int nodePort,
    const char* seed,
    uint64_t eventId, uint64_t option, uint64_t amount, int64_t price,
    uint64_t antiSpamAmount,
    uint32_t scheduledTickOffset)
{
    auto qc = make_qc(nodeIp, nodePort);

    qtryBasicInfo_output qbi{};
    quotteryGetBasicInfo(qc, qbi);
    antiSpamAmount = qbi.antiSpamAmount;

    LOG("\n-------------------------------------\n\n");
    LOG("Sending QTRY order action - functionNumber: %d\n", functionNumber);
    LOG("eventId: %" PRIu64 "\n", eventId);
    LOG("option: %" PRIu64 "\n", option);
    LOG("amount: %" PRIu64 "\n", amount);
    LOG("price: %" PRId64 "\n", price);
    LOG("antiSpamAmount: %" PRIu64 "\n", antiSpamAmount);
    LOG("\n-------------------------------------\n\n");

    qtryOrderAction_input input{};
    input.eventId = eventId;
    input.option = option;
    input.amount = amount;
    input.price = price;
    
    makeContractTransaction(nodeIp, nodePort, seed,
        QUOTTERY_CONTRACT_ID,
        functionNumber,
        /*amount=*/(int64_t)antiSpamAmount,
        sizeof(input), &input,
        scheduledTickOffset,
        &qc);
}

void qtryAddToAskOrder(const char* nodeIp, int nodePort, const char* seed,
    uint64_t eventId, uint64_t option, uint64_t amount, int64_t price,
    uint64_t antiSpamAmount, uint32_t scheduledTickOffset)
{
    qtryOrderAction<QTRY_ADD_ASK_ORDER>(nodeIp, nodePort, seed, eventId, option, amount, price, antiSpamAmount, scheduledTickOffset);
}

void qtryAddToBidOrder(const char* nodeIp, int nodePort, const char* seed,
    uint64_t eventId, uint64_t option, uint64_t amount, int64_t price,
    uint64_t antiSpamAmount, uint32_t scheduledTickOffset)
{
    qtryOrderAction<QTRY_ADD_BID_ORDER>(nodeIp, nodePort, seed, eventId, option, amount, price, antiSpamAmount, scheduledTickOffset);
}

void qtryRemoveAskOrder(const char* nodeIp, int nodePort, const char* seed,
    uint64_t eventId, uint64_t option, uint64_t amount, int64_t price,
    uint64_t antiSpamAmount, uint32_t scheduledTickOffset)
{
    qtryOrderAction<QTRY_REMOVE_ASK_ORDER>(nodeIp, nodePort, seed, eventId, option, amount, price, antiSpamAmount, scheduledTickOffset);
}

void qtryRemoveBidOrder(const char* nodeIp, int nodePort, const char* seed,
    uint64_t eventId, uint64_t option, uint64_t amount, int64_t price,
    uint64_t antiSpamAmount, uint32_t scheduledTickOffset)
{
    qtryOrderAction<QTRY_REMOVE_BID_ORDER>(nodeIp, nodePort, seed, eventId, option, amount, price, antiSpamAmount, scheduledTickOffset);
}

void qtryGetOrders(const char* nodeIp, int nodePort,
    uint64_t eventId, uint64_t option, uint64_t isBid, uint64_t offset,
    qtryGetOrders_output& result)
{
    qtryGetOrders_input input{};
    input.eventId = eventId;
    input.option = option;
    input.isBid = isBid;
    input.offset = offset;

    memset(&result, 0, sizeof(result));
    if (!runContractFunction(nodeIp, nodePort, QUOTTERY_CONTRACT_ID, QTRY_GET_ORDERS,
                             &input, sizeof(input), &result, sizeof(result)))
    {
        memset(&result, 0, sizeof(result));
    }
}

void quotteryPrintOrders(const char* nodeIp, int nodePort,
    uint64_t eventId, uint64_t option, uint64_t isBid, uint64_t offset)
{
    qtryGetOrders_output result;
    memset(&result, 0, sizeof(qtryGetOrders_output));
    qtryGetOrders(nodeIp, nodePort, eventId, option, isBid, offset, result);

    int N = sizeof(result.orders) / sizeof(result.orders[0]);
    LOG("%s orders for eventId %" PRIu64 " option %" PRIu64 " (offset %" PRIu64 "):\n",
        isBid ? "Bid" : "Ask", eventId, option, offset);
    LOG("Entity\t\t\t\t\t\t\t\tPrice\tAmount\n");
    for (int i = 0; i < N; i++)
    {
        if (isZeroPubkey(result.orders[i].qo.entity))
        {
            break;
        }
        char iden[128] = { 0 };
        getIdentityFromPublicKey(result.orders[i].qo.entity, iden, false);
        LOG("%s\t%" PRId64 "\t%" PRIu64 "\n", iden, result.orders[i].price, result.orders[i].qo.amount);
    }
}

struct getUserPosition_input
{
    uint8_t uid[32];
};

void quotteryGetUserPosition(const char* nodeIp, int nodePort, const char* identity, getUserPosition_output& result)
{
    getUserPosition_input input{};
    getPublicKeyFromIdentity(identity, input.uid);

    memset(&result, 0, sizeof(result));
    if (!runContractFunction(nodeIp, nodePort, QUOTTERY_CONTRACT_ID, QUOTTERY_GET_USER_POSITION,
                             &input, sizeof(input), &result, sizeof(result)))
    {
        memset(&result, 0, sizeof(result));
    }
}

void quotteryPrintUserPosition(const char* nodeIp, int nodePort, const char* identity)
{
    getUserPosition_output result;
    memset(&result, 0, sizeof(getUserPosition_output));
    quotteryGetUserPosition(nodeIp, nodePort, identity, result);

    LOG("Positions for %s (count: %" PRId64 "):\n", identity, result.count);
    LOG("EventId\tOption\tAmount\n");
    for (int64_t i = 0; i < result.count; i++)
    {
        uint64_t eventId = QUOTTERY_EO_GET_EVENTID(result.p[i].eo);
        uint64_t option = QUOTTERY_EO_GET_OPTION(result.p[i].eo);
        LOG("%" PRIu64 "\t%" PRIu64 "\t%" PRId64 "\n", eventId, option, result.p[i].amount);
    }
}

struct qtryPublishResult_input
{
    uint64_t eventId;
    uint64_t option;
};

struct qtryTryFinalizeEvent_input
{
    uint64_t eventId;
};

static bool isCurrentUtcAfterPackedDateTime(uint64_t packedDateTime)
{
    uint32_t endYear;
    uint8_t endMonth, endDay, endHour, endMinute, endSecond;
    uint16_t endMillisec, endMicrosec;
    unpackDateTime(endYear, endMonth, endDay, endHour, endMinute, endSecond, endMillisec, endMicrosec, packedDateTime);

    std::time_t nowTs = std::time(nullptr);
    std::tm nowUtc{};
#if defined(_WIN32)
    gmtime_s(&nowUtc, &nowTs);
#else
    gmtime_r(&nowTs, &nowUtc);
#endif

    const uint32_t nowYear = static_cast<uint32_t>(nowUtc.tm_year + 1900);
    const uint8_t nowMonth = static_cast<uint8_t>(nowUtc.tm_mon + 1);
    const uint8_t nowDay = static_cast<uint8_t>(nowUtc.tm_mday);
    const uint8_t nowHour = static_cast<uint8_t>(nowUtc.tm_hour);
    const uint8_t nowMinute = static_cast<uint8_t>(nowUtc.tm_min);
    const uint8_t nowSecond = static_cast<uint8_t>(nowUtc.tm_sec);

    uint64_t nowPacked = 0;
    packDateTime(nowYear, nowMonth, nowDay, nowHour, nowMinute, nowSecond, 0, 0, nowPacked);

    return nowPacked >= packedDateTime;
}

static bool quotteryParseUtcDateTime(
    const std::string& text,
    uint64_t& packedDateTime,
    std::time_t& timestamp)
{
    const char* value = text.c_str();
    if (text.size() != 19 || value[4] != '-' || value[7] != '-' || value[10] != ' ' ||
        value[13] != ':' || value[16] != ':')
    {
        LOG("Error: invalid date-time format. Expected: YYYY-MM-DD hh:mm:ss (UTC)\n");
        return false;
    }

    const int digitPositions[] = { 0, 1, 2, 3, 5, 6, 8, 9, 11, 12, 14, 15, 17, 18 };
    for (int position : digitPositions)
    {
        if (!isdigit(static_cast<unsigned char>(value[position])))
        {
            LOG("Error: invalid date-time format. Expected: YYYY-MM-DD hh:mm:ss (UTC)\n");
            return false;
        }
    }

    const uint32_t year = (value[0] - '0') * 1000 + (value[1] - '0') * 100 +
        (value[2] - '0') * 10 + (value[3] - '0');
    const uint8_t month = static_cast<uint8_t>((value[5] - '0') * 10 + (value[6] - '0'));
    const uint8_t day = static_cast<uint8_t>((value[8] - '0') * 10 + (value[9] - '0'));
    const uint8_t hour = static_cast<uint8_t>((value[11] - '0') * 10 + (value[12] - '0'));
    const uint8_t minute = static_cast<uint8_t>((value[14] - '0') * 10 + (value[15] - '0'));
    const uint8_t second = static_cast<uint8_t>((value[17] - '0') * 10 + (value[18] - '0'));

    std::tm utc{};
    utc.tm_year = static_cast<int>(year) - 1900;
    utc.tm_mon = month - 1;
    utc.tm_mday = day;
    utc.tm_hour = hour;
    utc.tm_min = minute;
    utc.tm_sec = second;
#if defined(_WIN32)
    timestamp = _mkgmtime(&utc);
#else
    timestamp = timegm(&utc);
#endif
    if (timestamp == static_cast<std::time_t>(-1) ||
        utc.tm_year != static_cast<int>(year) - 1900 || utc.tm_mon != month - 1 ||
        utc.tm_mday != day || utc.tm_hour != hour || utc.tm_min != minute || utc.tm_sec != second)
    {
        LOG("Error: invalid UTC date-time value: %s\n", text.c_str());
        return false;
    }

    packDateTime(year, month, day, hour, minute, second, 0, 0, packedDateTime);
    return true;
}

static bool quotteryValidateGameOperator(
    QCPtr& qc,
    const char* seed,
    qtryBasicInfo_output& basic)
{
    uint8_t subSeed[32] = {};
    uint8_t privateKey[32] = {};
    uint8_t sourcePublicKey[32] = {};
    getSubseedFromSeed(reinterpret_cast<const uint8_t*>(seed), subSeed);
    getPrivateKeyFromSubSeed(subSeed, privateKey);
    getPublicKeyFromPrivateKey(privateKey, sourcePublicKey);

    quotteryGetBasicInfo(qc, basic);
    if (isArrayZero(reinterpret_cast<uint8_t*>(&basic), sizeof(basic)))
    {
        LOG("Error: failed to get Quottery basic info\n");
        return false;
    }

    if (memcmp(sourcePublicKey, basic.gameOperator, sizeof(sourcePublicKey)) != 0)
    {
        char sourceIdentity[128] = {};
        char goIdentity[128] = {};
        getIdentityFromPublicKey(sourcePublicKey, sourceIdentity, false);
        getIdentityFromPublicKey(basic.gameOperator, goIdentity, false);
        LOG("Error: seed is not the game operator\n");
        LOG("Current identity: %s\n", sourceIdentity);
        LOG("Game operator:    %s\n", goIdentity);
        return false;
    }
    return true;
}

static bool quotteryGetEventGroupData(
    const char* nodeIp,
    int nodePort,
    uint64_t eventGroupId,
    GetEventGroup_output& result)
{
    GetEventGroup_input input{};
    input.eventGroupId = eventGroupId;
    memset(&result, 0, sizeof(result));
    return runContractFunction(nodeIp, nodePort, QUOTTERY_CONTRACT_ID, QTRY_GET_EVENT_GROUP,
                               &input, sizeof(input), &result, sizeof(result));
}

void qtryCreateEventGroup(
    const char* nodeIp,
    int nodePort,
    const char* seed,
    uint32_t scheduledTickOffset,
    const std::string& description,
    uint16_t expectedMarketCount,
    uint8_t mode)
{
    if (expectedMarketCount == 0 || expectedMarketCount > QUOTTERY_MAX_MARKETS_PER_EVENT_GROUP ||
        mode > QUOTTERY_EVENT_GROUP_MODE_EXCLUSIVE_ONE ||
        (mode == QUOTTERY_EVENT_GROUP_MODE_EXCLUSIVE_ONE && expectedMarketCount < 2))
    {
        LOG("Error: invalid event group mode or market count\n");
        return;
    }

    auto qc = make_qc(nodeIp, nodePort);
    qtryBasicInfo_output basic{};
    if (!quotteryValidateGameOperator(qc, seed, basic))
    {
        return;
    }

    CreateEventGroup_input input{};
    memcpy(input.desc, description.c_str(), std::min(description.size(), sizeof(input.desc)));
    input.expectedMarketCount = expectedMarketCount;
    input.mode = mode;

    LOG("Sending QTRY create event group\n");
    LOG("description: %s\n", description.c_str());
    LOG("expected markets: %u\n", expectedMarketCount);
    LOG("mode: %s (%u)\n", quotteryEventGroupModeName(mode), mode);

    makeContractTransaction(nodeIp, nodePort, seed,
        QUOTTERY_CONTRACT_ID, QTRY_CREATE_EVENT_GROUP, 0,
        sizeof(input), &input, scheduledTickOffset, &qc);
}

void qtryAddMarket(
    const char* nodeIp,
    int nodePort,
    const char* seed,
    uint32_t scheduledTickOffset,
    uint64_t eventGroupId,
    const std::string& marketDescription,
    const std::string& option0Description,
    const std::string& option1Description,
    const std::string& endDate,
    uint16_t tagId)
{
    GetEventGroup_output group{};
    if (!quotteryGetEventGroupData(nodeIp, nodePort, eventGroupId, group))
    {
        LOG("Error: failed to get event group %" PRIu64 "\n", eventGroupId);
        return;
    }
    if (!group.exists || group.eventGroupInfo.status != QUOTTERY_EVENT_GROUP_STATUS_DRAFT)
    {
        LOG("Error: event group %" PRIu64 " does not exist or is not in DRAFT state\n", eventGroupId);
        return;
    }
    if (group.eventGroupInfo.marketCount >= group.eventGroupInfo.expectedMarketCount)
    {
        LOG("Error: event group %" PRIu64 " already has all expected markets\n", eventGroupId);
        return;
    }

    uint64_t packedEndDate = 0;
    std::time_t endTimestamp = 0;
    if (!quotteryParseUtcDateTime(endDate, packedEndDate, endTimestamp))
    {
        return;
    }
    const std::time_t now = std::time(nullptr);
    if (endTimestamp <= now)
    {
        LOG("Error: market end date must be in the future\n");
        return;
    }

    auto qc = make_qc(nodeIp, nodePort);
    qtryBasicInfo_output basic{};
    if (!quotteryValidateGameOperator(qc, seed, basic))
    {
        return;
    }

    const uint64_t durationSeconds = static_cast<uint64_t>(endTimestamp - now);
    const uint64_t durationDays = (durationSeconds + 86399ULL) / 86400ULL;
    if (durationDays != 0 && basic.feePerDay > static_cast<uint64_t>(INT64_MAX) / durationDays)
    {
        LOG("Error: calculated market creation fee exceeds int64 range\n");
        return;
    }
    const int64_t fee = static_cast<int64_t>(durationDays * basic.feePerDay);

    AddMarket_input input{};
    input.eventGroupId = eventGroupId;
    input.qei.endDate = packedEndDate;
    memcpy(input.qei.desc, marketDescription.c_str(),
        std::min(marketDescription.size(), sizeof(input.qei.desc) - sizeof(tagId)));
    input.qei.desc[126] = static_cast<uint8_t>(tagId & 0xff);
    input.qei.desc[127] = static_cast<uint8_t>((tagId >> 8) & 0xff);
    memcpy(input.qei.option0Desc, option0Description.c_str(),
        std::min(option0Description.size(), sizeof(input.qei.option0Desc)));
    memcpy(input.qei.option1Desc, option1Description.c_str(),
        std::min(option1Description.size(), sizeof(input.qei.option1Desc)));

    LOG("Sending QTRY add market\n");
    LOG("event group ID: %" PRIu64 "\n", eventGroupId);
    LOG("end date: %s UTC\n", endDate.c_str());
    LOG("tag ID: %u\n", tagId);
    LOG("creation fee: %" PRId64 "\n", fee);

    makeContractTransaction(nodeIp, nodePort, seed,
        QUOTTERY_CONTRACT_ID, QTRY_ADD_MARKET, fee,
        sizeof(input), &input, scheduledTickOffset, &qc);
}

void qtryOpenEventGroup(
    const char* nodeIp,
    int nodePort,
    const char* seed,
    uint32_t scheduledTickOffset,
    uint64_t eventGroupId)
{
    GetEventGroup_output group{};
    if (!quotteryGetEventGroupData(nodeIp, nodePort, eventGroupId, group) || !group.exists)
    {
        LOG("Error: event group %" PRIu64 " does not exist\n", eventGroupId);
        return;
    }
    if (group.eventGroupInfo.status != QUOTTERY_EVENT_GROUP_STATUS_DRAFT ||
        group.eventGroupInfo.marketCount != group.eventGroupInfo.expectedMarketCount)
    {
        LOG("Error: event group must be DRAFT and contain all expected markets before opening\n");
        return;
    }

    auto qc = make_qc(nodeIp, nodePort);
    qtryBasicInfo_output basic{};
    if (!quotteryValidateGameOperator(qc, seed, basic))
    {
        return;
    }

    EventGroupId_input input{};
    input.eventGroupId = eventGroupId;
    LOG("Sending QTRY open event group %" PRIu64 "\n", eventGroupId);
    makeContractTransaction(nodeIp, nodePort, seed,
        QUOTTERY_CONTRACT_ID, QTRY_OPEN_EVENT, 0,
        sizeof(input), &input, scheduledTickOffset, &qc);
}

void qtryPublishEventResult(
    const char* nodeIp,
    int nodePort,
    const char* seed,
    uint32_t scheduledTickOffset,
    uint64_t eventGroupId,
    uint64_t winningMarketId)
{
    GetEventGroup_output group{};
    if (!quotteryGetEventGroupData(nodeIp, nodePort, eventGroupId, group) || !group.exists)
    {
        LOG("Error: event group %" PRIu64 " does not exist\n", eventGroupId);
        return;
    }
    if (group.eventGroupInfo.mode != QUOTTERY_EVENT_GROUP_MODE_EXCLUSIVE_ONE ||
        group.eventGroupInfo.status != QUOTTERY_EVENT_GROUP_STATUS_OPEN)
    {
        LOG("Error: group publish requires an OPEN EXCLUSIVE_ONE event group\n");
        return;
    }

    bool winningMarketFound = false;
    for (size_t i = 0; i < group.eventGroupInfo.marketCount && i < QUOTTERY_MAX_MARKETS_PER_EVENT_GROUP; ++i)
    {
        if (group.markets.marketIds[i] == winningMarketId)
        {
            winningMarketFound = true;
            break;
        }
    }
    if (!winningMarketFound)
    {
        LOG("Error: market %" PRIu64 " does not belong to event group %" PRIu64 "\n",
            winningMarketId, eventGroupId);
        return;
    }

    auto qc = make_qc(nodeIp, nodePort);
    qtryBasicInfo_output basic{};
    if (!quotteryValidateGameOperator(qc, seed, basic))
    {
        return;
    }
    if (basic.depositAmountForDispute > static_cast<uint64_t>(INT64_MAX))
    {
        LOG("Error: dispute deposit exceeds int64 range\n");
        return;
    }

    EventGroupResult_input input{};
    input.eventGroupId = eventGroupId;
    input.winningMarketId = winningMarketId;
    LOG("Sending QTRY publish event group result\n");
    LOG("event group ID: %" PRIu64 "\n", eventGroupId);
    LOG("winning market ID: %" PRIu64 "\n", winningMarketId);
    LOG("deposit: %" PRIu64 "\n", basic.depositAmountForDispute);
    makeContractTransaction(nodeIp, nodePort, seed,
        QUOTTERY_CONTRACT_ID, QTRY_PUBLISH_EVENT_RESULT,
        static_cast<int64_t>(basic.depositAmountForDispute),
        sizeof(input), &input, scheduledTickOffset, &qc);
}

void qtryDisputeEventResult(
    const char* nodeIp,
    int nodePort,
    const char* seed,
    uint32_t scheduledTickOffset,
    uint64_t eventGroupId)
{
    auto qc = make_qc(nodeIp, nodePort);
    qtryBasicInfo_output basic{};
    quotteryGetBasicInfo(qc, basic);
    if (isArrayZero(reinterpret_cast<uint8_t*>(&basic), sizeof(basic)))
    {
        LOG("Error: failed to get Quottery basic info\n");
        return;
    }
    if (basic.depositAmountForDispute > static_cast<uint64_t>(INT64_MAX))
    {
        LOG("Error: dispute deposit exceeds int64 range\n");
        return;
    }

    EventGroupId_input input{};
    input.eventGroupId = eventGroupId;
    LOG("Sending QTRY dispute event group result\n");
    LOG("event group ID: %" PRIu64 "\n", eventGroupId);
    LOG("deposit: %" PRIu64 "\n", basic.depositAmountForDispute);
    makeContractTransaction(nodeIp, nodePort, seed,
        QUOTTERY_CONTRACT_ID, QTRY_DISPUTE_EVENT_RESULT,
        static_cast<int64_t>(basic.depositAmountForDispute),
        sizeof(input), &input, scheduledTickOffset, &qc);
}

void qtryResolveEventDispute(
    const char* nodeIp,
    int nodePort,
    const char* seed,
    uint32_t scheduledTickOffset,
    uint64_t eventGroupId,
    uint64_t winningMarketId)
{
    EventGroupResult_input input{};
    input.eventGroupId = eventGroupId;
    input.winningMarketId = winningMarketId;
    LOG("Sending QTRY resolve event group dispute vote\n");
    LOG("event group ID: %" PRIu64 "\n", eventGroupId);
    LOG("winning market ID: %" PRIu64 "\n", winningMarketId);
    makeContractTransaction(nodeIp, nodePort, seed,
        QUOTTERY_CONTRACT_ID, QTRY_RESOLVE_EVENT_DISPUTE, 10000000,
        sizeof(input), &input, scheduledTickOffset);
}

void qtryCancelEventGroup(
    const char* nodeIp,
    int nodePort,
    const char* seed,
    uint32_t scheduledTickOffset,
    uint64_t eventGroupId)
{
    GetEventGroup_output group{};
    if (!quotteryGetEventGroupData(nodeIp, nodePort, eventGroupId, group) || !group.exists ||
        group.eventGroupInfo.status != QUOTTERY_EVENT_GROUP_STATUS_DRAFT)
    {
        LOG("Error: event group %" PRIu64 " does not exist or is not in DRAFT state\n", eventGroupId);
        return;
    }

    auto qc = make_qc(nodeIp, nodePort);
    qtryBasicInfo_output basic{};
    if (!quotteryValidateGameOperator(qc, seed, basic))
    {
        return;
    }

    EventGroupId_input input{};
    input.eventGroupId = eventGroupId;
    LOG("Sending QTRY cancel event group %" PRIu64 "\n", eventGroupId);
    makeContractTransaction(nodeIp, nodePort, seed,
        QUOTTERY_CONTRACT_ID, QTRY_CANCEL_EVENT_GROUP, 0,
        sizeof(input), &input, scheduledTickOffset, &qc);
}

void qtryPublishResult(const char* nodeIp, int nodePort, const char* seed, uint32_t scheduledTickOffset, uint64_t eventId, uint64_t result)
{
    if (result != 0 && result != 1)
    {
        LOG("Error: result can only be 0 or 1\n");
        return;
    }

    auto qc = make_qc(nodeIp, nodePort);

    uint8_t privateKey[32] = { 0 };
    uint8_t sourcePublicKey[32] = { 0 };
    uint8_t subSeed[32] = { 0 };
    char sourceIdentity[128] = { 0 };
    char goIdentity[128] = { 0 };

    getSubseedFromSeed((uint8_t*)seed, subSeed);
    getPrivateKeyFromSubSeed(subSeed, privateKey);
    getPublicKeyFromPrivateKey(privateKey, sourcePublicKey);
    getIdentityFromPublicKey(sourcePublicKey, sourceIdentity, false);

    qtryBasicInfo_output basic{};
    quotteryGetBasicInfo(qc, basic);

    if (isArrayZero((uint8_t*)&basic, sizeof(basic)))
    {
        LOG("Error: failed to get Quottery basic info\n");
        return;
    }

    if (memcmp(sourcePublicKey, basic.gameOperator, 32) != 0)
    {
        getIdentityFromPublicKey(basic.gameOperator, goIdentity, false);
        LOG("Error: seed is not the game operator\n");
        LOG("Current identity: %s\n", sourceIdentity);
        LOG("Game operator:    %s\n", goIdentity);
        return;
    }

    getEventInfo_output eventInfo{};
    _quotteryGetEventInfo(qc, eventId, eventInfo);

    if (isArrayZero((uint8_t*)&eventInfo, sizeof(eventInfo)))
    {
        LOG("Error: failed to get event info for eventId %" PRIu64 "\n", eventId);
        return;
    }

    if (eventInfo.qei.eid == (uint64_t)-1)
    {
        LOG("Error: eventId %" PRIu64 " does not exist\n", eventId);
        return;
    }

    if (!isCurrentUtcAfterPackedDateTime(eventInfo.qei.endDate))
    {
        uint32_t year;
        uint8_t month, day, hour, minute, second;
        uint16_t millisec, microsec;
        unpackDateTime(year, month, day, hour, minute, second, millisec, microsec, eventInfo.qei.endDate);
        LOG("Error: event %" PRIu64 " has not ended yet\n", eventId);
        LOG("End date (UTC): %04u-%02u-%02u %02u:%02u:%02u\n", year, month, day, hour, minute, second);
        return;
    }

    qtryPublishResult_input input{};
    input.eventId = eventId;
    input.option = result;

    LOG("\n-------------------------------------\n\n");
    LOG("Sending QTRY publish result\n");
    LOG("eventId: %" PRIu64 "\n", eventId);
    LOG("result: %" PRIu64 "\n", result);
    LOG("depositAmountForDispute: %" PRIu64 "\n", basic.depositAmountForDispute);
    LOG("\n-------------------------------------\n\n");

    makeContractTransaction(nodeIp, nodePort, seed,
        QUOTTERY_CONTRACT_ID,
        QTRY_PUBLISH_RESULT,
        /*amount=*/(int64_t)basic.depositAmountForDispute,
        sizeof(input), &input,
        scheduledTickOffset,
        &qc);
}

void qtryTryFinalizeEvent(const char* nodeIp, int nodePort, const char* seed, uint32_t scheduledTickOffset, uint64_t eventId)
{
    auto qc = make_qc(nodeIp, nodePort);

    uint8_t privateKey[32] = { 0 };
    uint8_t sourcePublicKey[32] = { 0 };
    uint8_t subSeed[32] = { 0 };
    char sourceIdentity[128] = { 0 };
    char goIdentity[128] = { 0 };

    getSubseedFromSeed((uint8_t*)seed, subSeed);
    getPrivateKeyFromSubSeed(subSeed, privateKey);
    getPublicKeyFromPrivateKey(privateKey, sourcePublicKey);
    getIdentityFromPublicKey(sourcePublicKey, sourceIdentity, false);

    qtryBasicInfo_output basic{};
    quotteryGetBasicInfo(qc, basic);

    if (isArrayZero((uint8_t*)&basic, sizeof(basic)))
    {
        LOG("Error: failed to get Quottery basic info\n");
        return;
    }

    if (memcmp(sourcePublicKey, basic.gameOperator, 32) != 0)
    {
        getIdentityFromPublicKey(basic.gameOperator, goIdentity, false);
        LOG("Error: seed is not the game operator\n");
        LOG("Current identity: %s\n", sourceIdentity);
        LOG("Game operator:    %s\n", goIdentity);
        return;
    }

    getEventInfo_output eventInfo{};
    _quotteryGetEventInfo(qc, eventId, eventInfo);

    if (isArrayZero((uint8_t*)&eventInfo, sizeof(eventInfo)))
    {
        LOG("Error: failed to get event info for eventId %" PRIu64 "\n", eventId);
        return;
    }

    if (eventInfo.qei.eid != eventId)
    {
        LOG("Error: eventId %" PRIu64 " does not exist\n", eventId);
        return;
    }

    if (eventInfo.resultByGO == -1)
    {
        LOG("Error: event %" PRIu64 " does not have a published result yet\n", eventId);
        return;
    }

    if (!isZeroPubkey(eventInfo.disputerInfo.pubkey))
    {
        LOG("Error: event %" PRIu64 " is under dispute and cannot be finalized\n", eventId);
        return;
    }

    const uint32_t currentTick = getTickNumberFromNode(qc);
    const uint32_t scheduledTick = currentTick + scheduledTickOffset;

    if (eventInfo.publishTickTime + 1000 > scheduledTick)
    {
        LOG("Error: event %" PRIu64 " cannot be finalized yet\n", eventId);
        LOG("Publish tick: %" PRIu32 "\n", eventInfo.publishTickTime);
        LOG("Earliest finalize tick: %" PRIu32 "\n", eventInfo.publishTickTime + 1000);
        LOG("Scheduled tick: %" PRIu32 "\n", scheduledTick);
        return;
    }

    qtryTryFinalizeEvent_input input{};
    input.eventId = eventId;

    LOG("\n-------------------------------------\n\n");
    LOG("Sending QTRY try finalize event\n");
    LOG("eventId: %" PRIu64 "\n", eventId);
    LOG("publishTickTime: %" PRIu32 "\n", eventInfo.publishTickTime);
    LOG("\n-------------------------------------\n\n");

    makeContractTransaction(nodeIp, nodePort, seed,
        QUOTTERY_CONTRACT_ID,
        QTRY_TRY_FINALIZE_EVENT,
        /*amount=*/0,
        sizeof(input), &input,
        scheduledTickOffset,
        &qc);
}

struct qtryDispute_input
{
    uint64_t eventId;
};

void qtryDispute(const char* nodeIp, int nodePort, const char* seed, uint32_t scheduledTickOffset, uint64_t eventId)
{
    auto qc = make_qc(nodeIp, nodePort);

    uint8_t privateKey[32] = { 0 };
    uint8_t sourcePublicKey[32] = { 0 };
    uint8_t subSeed[32] = { 0 };

    getSubseedFromSeed((uint8_t*)seed, subSeed);
    getPrivateKeyFromSubSeed(subSeed, privateKey);
    getPublicKeyFromPrivateKey(privateKey, sourcePublicKey);

    qtryBasicInfo_output basic{};
    quotteryGetBasicInfo(qc, basic);

    if (isArrayZero((uint8_t*)&basic, sizeof(basic)))
    {
        LOG("Error: failed to get Quottery basic info\n");
        return;
    }

    const uint64_t depositAmount = basic.depositAmountForDispute;

    // Fetch event info to validate dispute preconditions
    getEventInfo_output eventInfo{};
    _quotteryGetEventInfo(qc, eventId, eventInfo);

    if (isArrayZero((uint8_t*)&eventInfo, sizeof(eventInfo)))
    {
        LOG("Error: failed to get event info for eventId %" PRIu64 "\n", eventId);
        return;
    }

    if (eventInfo.qei.eid != eventId)
    {
        LOG("Error: eventId %" PRIu64 " does not exist\n", eventId);
        return;
    }

    if (eventInfo.resultByGO == -1)
    {
        LOG("Error: event %" PRIu64 " does not have a published result yet. Nothing to dispute.\n", eventId);
        return;
    }

    if (eventInfo.publishTickTime == 0xffffffffu)
    {
        LOG("Error: event %" PRIu64 " is already finalized. Cannot dispute.\n", eventId);
        return;
    }

    if (!isZeroPubkey(eventInfo.disputerInfo.pubkey))
    {
        char existingDisputer[128] = { 0 };
        getIdentityFromPublicKey(eventInfo.disputerInfo.pubkey, existingDisputer, false);
        LOG("Error: event %" PRIu64 " is already being disputed by %s\n", eventId, existingDisputer);
        return;
    }

    const uint32_t currentTick = getTickNumberFromNode(qc);
    const uint32_t scheduledTick = currentTick + scheduledTickOffset;

    if (eventInfo.publishTickTime + 1000 <= scheduledTick)
    {
        LOG("Warning: dispute window may have passed for event %" PRIu64 "\n", eventId);
        LOG("Publish tick: %" PRIu32 ", finalize eligible at tick: %" PRIu32 ", scheduled tick: %" PRIu32 "\n",
            eventInfo.publishTickTime, eventInfo.publishTickTime + 1000, scheduledTick);
        LOG("The event may already be finalized by the time this transaction executes.\n");
    }

    {
        long long balance = getBalanceNumber(qc, sourcePublicKey);
        if (balance < 0)
        {
            LOG("Error: failed to query balance\n");
            return;
        }
        if (static_cast<uint64_t>(balance) < depositAmount)
        {
            LOG("Error: insufficient balance for dispute deposit\n");
            LOG("Required: %" PRIu64 ", available: %lld\n", depositAmount, balance);
            return;
        }
    }

    qtryDispute_input input{};
    input.eventId = eventId;

    LOG("\n-------------------------------------\n\n");
    LOG("Sending QTRY Dispute\n");
    LOG("eventId: %" PRIu64 "\n", eventId);
    LOG("depositAmountForDispute: %" PRIu64 "\n", depositAmount);
    LOG("\n-------------------------------------\n\n");

    makeContractTransaction(nodeIp, nodePort, seed,
        QUOTTERY_CONTRACT_ID,
        QTRY_DISPUTE,
        /*amount=*/(int64_t)depositAmount,
        sizeof(input), &input,
        scheduledTickOffset,
        &qc);
}

struct qtryResolveDispute_input
{
    uint64_t eventId;
    int64_t vote;
};

void qtryResolveDispute(const char* nodeIp, int nodePort, const char* seed, uint32_t scheduledTickOffset, uint64_t eventId, int64_t vote)
{
    if (vote != 0 && vote != 1)
    {
        LOG("Error: vote must be 0 or 1\n");
        return;
    }

    auto qc = make_qc(nodeIp, nodePort);

    uint8_t privateKey[32] = { 0 };
    uint8_t sourcePublicKey[32] = { 0 };
    uint8_t subSeed[32] = { 0 };
    char sourceIdentity[128] = { 0 };

    getSubseedFromSeed((uint8_t*)seed, subSeed);
    getPrivateKeyFromSubSeed(subSeed, privateKey);
    getPublicKeyFromPrivateKey(privateKey, sourcePublicKey);
    getIdentityFromPublicKey(sourcePublicKey, sourceIdentity, false);

    getEventInfo_output eventInfo{};
    _quotteryGetEventInfo(qc, eventId, eventInfo);

    if (isArrayZero((uint8_t*)&eventInfo, sizeof(eventInfo)))
    {
        LOG("Error: failed to get event info for eventId %" PRIu64 "\n", eventId);
        return;
    }

    if (eventInfo.qei.eid != eventId)
    {
        LOG("Error: eventId %" PRIu64 " does not exist\n", eventId);
        return;
    }

    if (isZeroPubkey(eventInfo.disputerInfo.pubkey))
    {
        LOG("Error: event %" PRIu64 " is not under dispute\n", eventId);
        return;
    }

    constexpr int64_t MIN_INVOCATION_REWARD = 10000000;

    {
        long long balance = getBalanceNumber(qc, sourcePublicKey);
        if (balance < 0)
        {
            LOG("Error: failed to query balance\n");
            return;
        }
        if (static_cast<uint64_t>(balance) < static_cast<uint64_t>(MIN_INVOCATION_REWARD))
        {
            LOG("Error: insufficient balance\n");
            LOG("Required (refunded if computor): %" PRId64 ", available: %lld\n", MIN_INVOCATION_REWARD, balance);
            return;
        }
    }

    qtryResolveDispute_input input{};
    input.eventId = eventId;
    input.vote = vote;

    LOG("\n-------------------------------------\n\n");
    LOG("Sending QTRY ResolveDispute\n");
    LOG("Caller: %s\n", sourceIdentity);
    LOG("eventId: %" PRIu64 "\n", eventId);
    LOG("vote: %" PRId64 " (%s)\n", vote, vote == 0 ? "No" : "Yes");
    LOG("invocationReward: %" PRId64 " (refunded if caller is a computor)\n", MIN_INVOCATION_REWARD);
    LOG("\n-------------------------------------\n\n");

    makeContractTransaction(nodeIp, nodePort, seed,
        QUOTTERY_CONTRACT_ID,
        QTRY_RESOLVE_DISPUTE,
        /*amount=*/MIN_INVOCATION_REWARD,
        sizeof(input), &input,
        scheduledTickOffset,
        &qc);
    LOG("Note: only computors can resolve disputes. If the caller is not a computor, the invocation reward will NOT be refunded.\n");
}

struct qtryUserClaimReward_input
{
    uint64_t eventId;
};

void qtryUserClaimReward(const char* nodeIp, int nodePort, const char* seed, uint32_t scheduledTickOffset, uint64_t eventId)
{
    auto qc = make_qc(nodeIp, nodePort);

    uint8_t privateKey[32] = { 0 };
    uint8_t sourcePublicKey[32] = { 0 };
    uint8_t subSeed[32] = { 0 };
    char sourceIdentity[128] = { 0 };

    getSubseedFromSeed((uint8_t*)seed, subSeed);
    getPrivateKeyFromSubSeed(subSeed, privateKey);
    getPublicKeyFromPrivateKey(privateKey, sourcePublicKey);
    getIdentityFromPublicKey(sourcePublicKey, sourceIdentity, false);

    getEventInfo_output eventInfo{};
    _quotteryGetEventInfo(qc, eventId, eventInfo);

    if (isArrayZero((uint8_t*)&eventInfo, sizeof(eventInfo)))
    {
        LOG("Error: failed to get event info for eventId %" PRIu64 "\n", eventId);
        return;
    }

    if (eventInfo.qei.eid != eventId)
    {
        LOG("Error: eventId %" PRIu64 " does not exist\n", eventId);
        return;
    }

    if (eventInfo.resultByGO == -1)
    {
        LOG("Error: event %" PRIu64 " does not have a result yet. Cannot claim reward.\n", eventId);
        return;
    }

    if (eventInfo.publishTickTime != 0xffffffffu)
    {
        if (!isZeroPubkey(eventInfo.disputerInfo.pubkey))
        {
            LOG("Warning: event %" PRIu64 " is still under dispute. The result may change.\n", eventId);
        }
        else
        {
            LOG("Warning: event %" PRIu64 " may not be finalized yet (publishTickTime: %" PRIu32 ").\n",
                eventId, eventInfo.publishTickTime);
            LOG("The SC will reject the claim if the result is not set.\n");
        }
    }

    {
        getUserPosition_output posResult{};
        quotteryGetUserPosition(nodeIp, nodePort, sourceIdentity, posResult);

        bool hasPosition = false;
        for (int64_t idx = 0; idx < posResult.count; idx++)
        {
            uint64_t posEventId = QUOTTERY_EO_GET_EVENTID(posResult.p[idx].eo);
            if (posEventId == eventId)
            {
                uint64_t posOption = QUOTTERY_EO_GET_OPTION(posResult.p[idx].eo);
                LOG("Found position in event %" PRIu64 ": option %" PRIu64 ", amount %" PRId64 "\n",
                    eventId, posOption, posResult.p[idx].amount);
                hasPosition = true;
                break;
            }
        }

        if (!hasPosition)
        {
            LOG("Error: no position found for %s in event %" PRIu64 "\n", sourceIdentity, eventId);
            LOG("You must have a position in this event to claim a reward.\n");
            return;
        }
    }

    constexpr int64_t CLAIM_INVOCATION_REWARD = 1000000;

    {
        long long balance = getBalanceNumber(qc, sourcePublicKey);
        if (balance < 0)
        {
            LOG("Error: failed to query balance\n");
            return;
        }
        if (static_cast<uint64_t>(balance) < static_cast<uint64_t>(CLAIM_INVOCATION_REWARD))
        {
            LOG("Error: insufficient balance\n");
            LOG("Required: %" PRId64 " (refunded if you have a winning position), available: %lld\n",
                CLAIM_INVOCATION_REWARD, balance);
            return;
        }
    }

    qtryUserClaimReward_input input{};
    input.eventId = eventId;

    LOG("\n-------------------------------------\n\n");
    LOG("Sending QTRY UserClaimReward\n");
    LOG("Caller: %s\n", sourceIdentity);
    LOG("eventId: %" PRIu64 "\n", eventId);
    LOG("invocationReward: %" PRId64 " (refunded if winning position exists)\n", CLAIM_INVOCATION_REWARD);
    LOG("\n-------------------------------------\n\n");

    makeContractTransaction(nodeIp, nodePort, seed,
        QUOTTERY_CONTRACT_ID,
        QTRY_USER_CLAIM_REWARD,
        /*amount=*/CLAIM_INVOCATION_REWARD,
        sizeof(input), &input,
        scheduledTickOffset,
        &qc);
    LOG("Note: the 1,000,000 qu fee is only refunded if you have a winning position. Otherwise it is lost (anti-spam).\n");
}

struct qtryGOForceClaimReward_input
{
    uint64_t eventId;
    uint8_t pubkeys[16][32]; // Array<id, 16>
};

void qtryGOForceClaimReward(const char* nodeIp, int nodePort, const char* seed,
    uint32_t scheduledTickOffset, uint64_t eventId,
    const char* identities[], int identityCount)
{
    if (identityCount <= 0 || identityCount > 16)
    {
        LOG("Error: must provide between 1 and 16 public key identities\n");
        return;
    }

    auto qc = make_qc(nodeIp, nodePort);

    uint8_t privateKey[32] = { 0 };
    uint8_t sourcePublicKey[32] = { 0 };
    uint8_t subSeed[32] = { 0 };
    char sourceIdentity[128] = { 0 };
    char goIdentity[128] = { 0 };

    getSubseedFromSeed((uint8_t*)seed, subSeed);
    getPrivateKeyFromSubSeed(subSeed, privateKey);
    getPublicKeyFromPrivateKey(privateKey, sourcePublicKey);
    getIdentityFromPublicKey(sourcePublicKey, sourceIdentity, false);

    qtryBasicInfo_output basic{};
    quotteryGetBasicInfo(qc, basic);

    if (isArrayZero((uint8_t*)&basic, sizeof(basic)))
    {
        LOG("Error: failed to get Quottery basic info\n");
        return;
    }

    if (memcmp(sourcePublicKey, basic.gameOperator, 32) != 0)
    {
        getIdentityFromPublicKey(basic.gameOperator, goIdentity, false);
        LOG("Error: seed is not the game operator\n");
        LOG("Current identity: %s\n", sourceIdentity);
        LOG("Game operator:    %s\n", goIdentity);
        return;
    }

    getEventInfo_output eventInfo{};
    _quotteryGetEventInfo(qc, eventId, eventInfo);

    if (isArrayZero((uint8_t*)&eventInfo, sizeof(eventInfo)))
    {
        LOG("Error: failed to get event info for eventId %" PRIu64 "\n", eventId);
        return;
    }

    if (eventInfo.qei.eid != eventId)
    {
        LOG("Error: eventId %" PRIu64 " does not exist\n", eventId);
        return;
    }

    if (eventInfo.resultByGO == -1)
    {
        LOG("Error: event %" PRIu64 " does not have a result yet. Cannot force claim.\n", eventId);
        return;
    }

    qtryGOForceClaimReward_input input{};
    input.eventId = eventId;
    memset(input.pubkeys, 0, sizeof(input.pubkeys));

    LOG("\n-------------------------------------\n\n");
    LOG("Sending QTRY GOForceClaimReward\n");
    LOG("Caller (GO): %s\n", sourceIdentity);
    LOG("eventId: %" PRIu64 "\n", eventId);
    LOG("Number of pubkeys: %d\n", identityCount);

    for (int idx = 0; idx < identityCount; idx++)
    {
        getPublicKeyFromIdentity(identities[idx], input.pubkeys[idx]);
        char verifyId[128] = { 0 };
        getIdentityFromPublicKey(input.pubkeys[idx], verifyId, false);
        LOG("  [%d] %s\n", idx, verifyId);
    }
    LOG("\n-------------------------------------\n\n");

    makeContractTransaction(nodeIp, nodePort, seed,
        QUOTTERY_CONTRACT_ID,
        QTRY_GO_FORCE_CLAIM_REWARD,
        /*amount=*/0,
        sizeof(input), &input,
        scheduledTickOffset,
        &qc);
}

struct qtryTransferShareManagementRights_input
{
    uint8_t issuer[32];    // Asset.issuer (id = 32 bytes public key)
    uint64_t assetName;    // Asset.assetName (uint64 encoded)
    int64_t numberOfShares;
    uint32_t newManagingContractIndex;
    uint32_t _padding;     // align to 8 bytes
};

static uint64_t encodeAssetName(const char* name)
{
    uint64_t result = 0;
    for (int i = 0; i < 7 && name[i] != '\0'; i++)
    {
        char c = name[i];
        // Qubic asset names are uppercase A-Z mapped to values 1-26
        if (c >= 'a' && c <= 'z')
            c = c - 'a' + 'A';
        if (c < 'A' || c > 'Z')
        {
            LOG("Error: invalid character '%c' in asset name. Only A-Z allowed.\n", name[i]);
            return 0;
        }
        result |= (static_cast<uint64_t>(c - 'A' + 1)) << (i * 8);
    }
    return result;
}

void qtryTransferShareManagementRights(const char* nodeIp, int nodePort, const char* seed,
    uint32_t scheduledTickOffset,
    const char* issuerIdentity, const char* assetName,
    int64_t numberOfShares, uint32_t newManagingContractIndex)
{
    if (numberOfShares <= 0)
    {
        LOG("Error: numberOfShares must be positive\n");
        return;
    }

    uint64_t encodedAssetName = encodeAssetName(assetName);
    if (encodedAssetName == 0)
    {
        LOG("Error: failed to encode asset name '%s'\n", assetName);
        return;
    }

    qtryTransferShareManagementRights_input input{};
    getPublicKeyFromIdentity(issuerIdentity, input.issuer);
    input.assetName = encodedAssetName;
    input.numberOfShares = numberOfShares;
    input.newManagingContractIndex = newManagingContractIndex;

    char issuerBuf[128] = { 0 };
    getIdentityFromPublicKey(input.issuer, issuerBuf, false);

    LOG("\n-------------------------------------\n\n");
    LOG("Sending QTRY TransferShareManagementRights\n");
    LOG("Asset issuer: %s\n", issuerBuf);
    LOG("Asset name: %s (encoded: %" PRIu64 ")\n", assetName, encodedAssetName);
    LOG("Number of shares: %" PRId64 "\n", numberOfShares);
    LOG("New managing contract index: %" PRIu32 "\n", newManagingContractIndex);
    LOG("\n-------------------------------------\n\n");

    makeContractTransaction(nodeIp, nodePort, seed,
        QUOTTERY_CONTRACT_ID,
        QTRY_TRANSFER_SHARE_MANAGEMENT_RIGHTS,
        /*amount=*/0,
        sizeof(input), &input,
        scheduledTickOffset);
}

void qtryCleanMemory(const char* nodeIp, int nodePort, const char* seed, uint32_t scheduledTickOffset)
{
    auto qc = make_qc(nodeIp, nodePort);

    uint8_t privateKey[32] = { 0 };
    uint8_t sourcePublicKey[32] = { 0 };
    uint8_t subSeed[32] = { 0 };
    char sourceIdentity[128] = { 0 };
    char goIdentity[128] = { 0 };

    getSubseedFromSeed((uint8_t*)seed, subSeed);
    getPrivateKeyFromSubSeed(subSeed, privateKey);
    getPublicKeyFromPrivateKey(privateKey, sourcePublicKey);
    getIdentityFromPublicKey(sourcePublicKey, sourceIdentity, false);

    qtryBasicInfo_output basic{};
    quotteryGetBasicInfo(qc, basic);

    if (isArrayZero((uint8_t*)&basic, sizeof(basic)))
    {
        LOG("Error: failed to get Quottery basic info\n");
        return;
    }

    if (memcmp(sourcePublicKey, basic.gameOperator, 32) != 0)
    {
        getIdentityFromPublicKey(basic.gameOperator, goIdentity, false);
        LOG("Error: seed is not the game operator\n");
        LOG("Current identity: %s\n", sourceIdentity);
        LOG("Game operator:    %s\n", goIdentity);
        return;
    }

    LOG("\n-------------------------------------\n\n");
    LOG("Sending QTRY CleanMemory\n");
    LOG("Caller (GO): %s\n", sourceIdentity);
    LOG("\n-------------------------------------\n\n");

    makeContractTransaction(nodeIp, nodePort, seed,
        QUOTTERY_CONTRACT_ID,
        QTRY_CLEAN_MEMORY,
        /*amount=*/0,
        /*extraDataSize=*/0, nullptr,
        scheduledTickOffset,
        &qc);
    LOG("This will finalize events with published results (past dispute window) and clean up state.\n");
}

struct qtryTransferQUSD_input
{
    uint8_t receiver[32];
    int64_t amount;
};

void qtryTransferQUSD(const char* nodeIp, int nodePort, const char* seed,
    const char* receiverIdentity, int64_t amount,
    uint32_t scheduledTickOffset)
{
    qtryTransferQUSD_input input{};
    getPublicKeyFromIdentity(receiverIdentity, input.receiver);
    input.amount = amount;

    char receiverBuf[128] = { 0 };
    getIdentityFromPublicKey(input.receiver, receiverBuf, false);

    LOG("\n-------------------------------------\n\n");
    LOG("Sending QTRY TransferQUSD (GARTH transfer via Quottery contract)\n");
    LOG("Receiver: %s\n", receiverBuf);
    LOG("Amount: %" PRId64 "\n", amount);
    LOG("\n-------------------------------------\n\n");

    makeContractTransaction(nodeIp, nodePort, seed,
        QUOTTERY_CONTRACT_ID,
        QTRY_TRANSFER_QUSD,
        /*amount=*/0,
        sizeof(input), &input,
        scheduledTickOffset);
}

// TransferQTRYGOV (proc 15)
struct qtryTransferQTRYGOV_input
{
    uint8_t receiver[32];
    int64_t amount;
};

void qtryTransferQTRYGOV(const char* nodeIp, int nodePort, const char* seed,
    const char* receiverIdentity, int64_t amount,
    uint32_t scheduledTickOffset)
{
    if (amount <= 0)
    {
        LOG("Error: amount must be positive\n");
        return;
    }

    qtryTransferQTRYGOV_input input{};
    getPublicKeyFromIdentity(receiverIdentity, input.receiver);
    input.amount = amount;

    char receiverBuf[128] = { 0 };
    getIdentityFromPublicKey(input.receiver, receiverBuf, false);

    LOG("\n-------------------------------------\n\n");
    LOG("Sending QTRY TransferQTRYGOV\n");
    LOG("Receiver: %s\n", receiverBuf);
    LOG("Amount: %" PRId64 "\n", amount);
    LOG("\n-------------------------------------\n\n");

    makeContractTransaction(nodeIp, nodePort, seed,
        QUOTTERY_CONTRACT_ID,
        QTRY_TRANSFER_QTRYGOV,
        /*amount=*/0,
        sizeof(input), &input,
        scheduledTickOffset);
}

// UpdateFeeDiscountList (proc 20)
struct qtryUpdateFeeDiscountList_input
{
    uint8_t userId[32];
    uint64_t newFeeRate;
    uint64_t ops; // 0 = remove, 1 = set
};

void qtryUpdateFeeDiscountList(const char* nodeIp, int nodePort, const char* seed,
    uint32_t scheduledTickOffset,
    const char* userIdentity, uint64_t newFeeRate, uint64_t ops)
{
    if (ops != 0 && ops != 1)
    {
        LOG("Error: ops must be 0 (remove) or 1 (set)\n");
        return;
    }

    if (ops == 1 && newFeeRate > 1000)
    {
        LOG("Error: newFeeRate must be <= 1000 (100%%)\n");
        return;
    }

    auto qc = make_qc(nodeIp, nodePort);

    uint8_t privateKey[32] = { 0 };
    uint8_t sourcePublicKey[32] = { 0 };
    uint8_t subSeed[32] = { 0 };
    char sourceIdentity[128] = { 0 };
    char goIdentity[128] = { 0 };

    getSubseedFromSeed((uint8_t*)seed, subSeed);
    getPrivateKeyFromSubSeed(subSeed, privateKey);
    getPublicKeyFromPrivateKey(privateKey, sourcePublicKey);
    getIdentityFromPublicKey(sourcePublicKey, sourceIdentity, false);

    qtryBasicInfo_output basic{};
    quotteryGetBasicInfo(qc, basic);

    if (memcmp(sourcePublicKey, basic.gameOperator, 32) != 0)
    {
        getIdentityFromPublicKey(basic.gameOperator, goIdentity, false);
        LOG("Error: seed is not the game operator\n");
        LOG("Current identity: %s\n", sourceIdentity);
        LOG("Game operator:    %s\n", goIdentity);
        return;
    }

    qtryUpdateFeeDiscountList_input input{};
    getPublicKeyFromIdentity(userIdentity, input.userId);
    input.newFeeRate = newFeeRate;
    input.ops = ops;

    char userBuf[128] = { 0 };
    getIdentityFromPublicKey(input.userId, userBuf, false);

    LOG("\n-------------------------------------\n\n");
    LOG("Sending QTRY UpdateFeeDiscountList\n");
    LOG("User: %s\n", userBuf);
    LOG("Operation: %s\n", ops == 0 ? "remove" : "set");
    if (ops == 1) LOG("New fee rate: %" PRIu64 " (%.1f%%)\n", newFeeRate, newFeeRate / 10.0);
    LOG("\n-------------------------------------\n\n");

    makeContractTransaction(nodeIp, nodePort, seed,
        QUOTTERY_CONTRACT_ID,
        QTRY_UPDATE_FEE_DISCOUNT_LIST,
        /*amount=*/0,
        sizeof(input), &input,
        scheduledTickOffset,
        &qc);
}

// ProposalVote (proc 100)
struct qtryProposalVote_input
{
    QtryGOV_cli proposed;
};

void qtryProposalVote(const char* nodeIp, int nodePort, const char* seed,
    uint32_t scheduledTickOffset,
    uint64_t operationFee, uint64_t shareholderFee, uint64_t burnFee,
    int64_t feePerDay, int64_t depositAmountForDispute,
    const char* operationIdIdentity)
{
    auto qc = make_qc(nodeIp, nodePort);

    uint8_t privateKey[32] = { 0 };
    uint8_t sourcePublicKey[32] = { 0 };
    uint8_t subSeed[32] = { 0 };
    char sourceIdentity[128] = { 0 };

    getSubseedFromSeed((uint8_t*)seed, subSeed);
    getPrivateKeyFromSubSeed(subSeed, privateKey);
    getPublicKeyFromPrivateKey(privateKey, sourcePublicKey);
    getIdentityFromPublicKey(sourcePublicKey, sourceIdentity, false);

    qtryBasicInfo_output basic{};
    quotteryGetBasicInfo(qc, basic);
    const int64_t txAmount = static_cast<int64_t>(basic.antiSpamAmount);

    qtryProposalVote_input input{};
    input.proposed.mOperationFee = operationFee;
    input.proposed.mShareHolderFee = shareholderFee;
    input.proposed.mBurnFee = burnFee;
    input.proposed.feePerDay = feePerDay;
    input.proposed.mDepositAmountForDispute = depositAmountForDispute;
    getPublicKeyFromIdentity(operationIdIdentity, input.proposed.mOperationId);

    char opIdBuf[128] = { 0 };
    getIdentityFromPublicKey(input.proposed.mOperationId, opIdBuf, false);

    LOG("\n-------------------------------------\n\n");
    LOG("Sending QTRY ProposalVote\n");
    LOG("Caller: %s\n", sourceIdentity);
    LOG("Proposed params:\n");
    LOG("  operationFee: %" PRIu64 " (%.1f%%)\n", operationFee, operationFee / 10.0);
    LOG("  shareholderFee: %" PRIu64 " (%.1f%%)\n", shareholderFee, shareholderFee / 10.0);
    LOG("  burnFee: %" PRIu64 " (%.1f%%)\n", burnFee, burnFee / 10.0);
    LOG("  feePerDay: %" PRId64 "\n", feePerDay);
    LOG("  depositAmountForDispute: %" PRId64 "\n", depositAmountForDispute);
    LOG("  operationId: %s\n", opIdBuf);
    LOG("\n-------------------------------------\n\n");

    makeContractTransaction(nodeIp, nodePort, seed,
        QUOTTERY_CONTRACT_ID,
        QTRY_PROPOSAL_VOTE,
        /*amount=*/txAmount,
        sizeof(input), &input,
        scheduledTickOffset,
        &qc);
}

// GetApprovedAmount (fn 7)
void quotteryGetApprovedAmount(const char* nodeIp, int nodePort, const char* identity)
{
    getApprovedAmount_input input{};
    getPublicKeyFromIdentity(identity, input.pk);

    getApprovedAmount_output result{};
    if (runContractFunction(nodeIp, nodePort, QUOTTERY_CONTRACT_ID, QTRY_GET_APPROVED_AMOUNT,
                            &input, sizeof(input), &result, sizeof(result)))
    {
        LOG("Approved QUSD amount for %s: %" PRIu64 "\n", identity, result.amount);
    }
    else
    {
        LOG("Failed to get approved amount\n");
    }
}

// GetTopProposals (fn 8)
void quotteryGetTopProposals(const char* nodeIp, int nodePort)
{
    getTopProposals_output result{};
    if (!runContractFunction(nodeIp, nodePort, QUOTTERY_CONTRACT_ID, QTRY_GET_TOP_PROPOSALS,
                             nullptr, 0, &result, sizeof(result)))
    {
        LOG("Failed to get top proposals\n");
        return;
    }
    
    LOG("Top governance proposals (%" PRId32 " unique this epoch):\n", result.uniqueCount);
    for (int i = 0; i < 3 && i < result.uniqueCount; i++)
    {
        const auto& p = result.top[i];
        if (p.totalVotes == 0) break;
        char opId[128] = { 0 };
        getIdentityFromPublicKey(p.proposed.mOperationId, opId, false);
        LOG("\n  #%d (%" PRId64 " votes):\n", i + 1, p.totalVotes);
        LOG("    operationFee: %" PRIu64 " (%.1f%%)\n", p.proposed.mOperationFee, p.proposed.mOperationFee / 10.0);
        LOG("    shareholderFee: %" PRIu64 " (%.1f%%)\n", p.proposed.mShareHolderFee, p.proposed.mShareHolderFee / 10.0);
        LOG("    burnFee: %" PRIu64 " (%.1f%%)\n", p.proposed.mBurnFee, p.proposed.mBurnFee / 10.0);
        LOG("    feePerDay: %" PRId64 "\n", p.proposed.feePerDay);
        LOG("    depositAmountForDispute: %" PRId64 "\n", p.proposed.mDepositAmountForDispute);
        LOG("    operationId: %s\n", opId);
    }
    if (result.uniqueCount == 0) LOG("  (none)\n");
}

static bool hasRequiredParameters(int currentIndex, int argc, int requiredCount, const char* command)
{
    const int expectedArgc = currentIndex + requiredCount + 1;
    if (expectedArgc != argc)
    {
        LOG("Error: %s expects %d parameter(s)\n", command, requiredCount);
        return false;
    }
    return true;
}

static bool hasAtLeastRequiredParameters(int currentIndex, int argc, int requiredCount, const char* command)
{
    if (currentIndex + requiredCount >= argc)
    {
        LOG("Error: %s expects at least %d parameter(s)\n", command, requiredCount);
        return false;
    }
    return true;
}

static bool tryParseUint64Arg(const char* text, uint64_t& value, const char* fieldName)
{
    if (text == nullptr || *text == '\0')
    {
        LOG("Error: %s is empty\n", fieldName);
        return false;
    }

    char* endPtr = nullptr;
    const unsigned long long parsed = std::strtoull(text, &endPtr, 10);
    if (*endPtr != '\0')
    {
        LOG("Error: invalid numeric value for %s: %s\n", fieldName, text);
        return false;
    }

    value = static_cast<uint64_t>(parsed);
    return true;
}

void quotteryEntryPoint(int argc, char** argv, const char* nodeIp, int nodePort, const char* seed, uint32_t scheduledTickOffset)
{
    constexpr uint64_t defaultAntiSpamAmount = 0;

    int i = 0;
    while (i < argc)
    {
        if (strcmp(argv[i], "getbasicinfo") == 0)
        {
            if (!hasRequiredParameters(i, argc, 0, "getbasicinfo"))
                return;
            quotteryPrintBasicInfo(nodeIp, nodePort);
            return;
        }

        if (strcmp(argv[i], "getactiveeventsid") == 0)
        {
            if (!hasRequiredParameters(i, argc, 0, "getactiveeventsid"))
                return;

            quotteryPrintActiveEvents(nodeIp, nodePort);
            return;
        }

        if (strcmp(argv[i], "createevent") == 0)
        {
            if (!hasRequiredParameters(i, argc, 5, "createevent"))
                return;

            const std::string eventDesc = argv[i + 1];
            const std::string opt0Desc = argv[i + 2];
            const std::string opt1Desc = argv[i + 3];
            const std::string endDate = argv[i + 4];

            uint64_t tagIdRaw = 0;
            if (!tryParseUint64Arg(argv[i + 5], tagIdRaw, "tagId"))
                return;
            uint16_t tagId = static_cast<uint16_t>(tagIdRaw);

            quotteryCreateEvent(nodeIp, nodePort, seed,
                eventDesc,
                opt0Desc,
                opt1Desc,
                endDate,
                tagId,
                scheduledTickOffset);
            return;
        }

        if (strcmp(argv[i], "createeventgroup") == 0)
        {
            if (!hasRequiredParameters(i, argc, 3, "createeventgroup"))
                return;

            uint64_t expectedMarketCountRaw = 0;
            if (!tryParseUint64Arg(argv[i + 2], expectedMarketCountRaw, "expectedMarketCount") ||
                expectedMarketCountRaw > UINT16_MAX)
            {
                LOG("Error: expectedMarketCount must fit uint16\n");
                return;
            }

            uint8_t mode = 0;
            if (strcmp(argv[i + 3], "independent") == 0 || strcmp(argv[i + 3], "0") == 0)
            {
                mode = QUOTTERY_EVENT_GROUP_MODE_INDEPENDENT;
            }
            else if (strcmp(argv[i + 3], "exclusive_one") == 0 || strcmp(argv[i + 3], "1") == 0)
            {
                mode = QUOTTERY_EVENT_GROUP_MODE_EXCLUSIVE_ONE;
            }
            else
            {
                LOG("Error: mode must be independent, exclusive_one, 0, or 1\n");
                return;
            }

            qtryCreateEventGroup(nodeIp, nodePort, seed, scheduledTickOffset,
                argv[i + 1], static_cast<uint16_t>(expectedMarketCountRaw), mode);
            return;
        }

        if (strcmp(argv[i], "addmarket") == 0)
        {
            if (!hasRequiredParameters(i, argc, 6, "addmarket"))
                return;

            uint64_t eventGroupId = 0;
            uint64_t tagIdRaw = 0;
            if (!tryParseUint64Arg(argv[i + 1], eventGroupId, "eventGroupId") ||
                !tryParseUint64Arg(argv[i + 6], tagIdRaw, "tagId"))
            {
                return;
            }
            if (tagIdRaw > UINT16_MAX)
            {
                LOG("Error: tagId must fit uint16\n");
                return;
            }

            qtryAddMarket(nodeIp, nodePort, seed, scheduledTickOffset,
                eventGroupId, argv[i + 2], argv[i + 3], argv[i + 4], argv[i + 5],
                static_cast<uint16_t>(tagIdRaw));
            return;
        }

        if (strcmp(argv[i], "openevent") == 0 || strcmp(argv[i], "openeventgroup") == 0)
        {
            if (!hasRequiredParameters(i, argc, 1, "openevent"))
                return;

            uint64_t eventGroupId = 0;
            if (!tryParseUint64Arg(argv[i + 1], eventGroupId, "eventGroupId"))
                return;

            qtryOpenEventGroup(nodeIp, nodePort, seed, scheduledTickOffset, eventGroupId);
            return;
        }

        if (strcmp(argv[i], "canceleventgroup") == 0)
        {
            if (!hasRequiredParameters(i, argc, 1, "canceleventgroup"))
                return;

            uint64_t eventGroupId = 0;
            if (!tryParseUint64Arg(argv[i + 1], eventGroupId, "eventGroupId"))
                return;

            qtryCancelEventGroup(nodeIp, nodePort, seed, scheduledTickOffset, eventGroupId);
            return;
        }

        if (strcmp(argv[i], "order") == 0)
        {
            if (!hasRequiredParameters(i, argc, 6, "order"))
                return;

            const char* action = argv[i + 1];
            const char* side = argv[i + 2];

            uint64_t eventId = 0;
            uint64_t option = 0;
            uint64_t amount = 0;
            uint64_t price = 0;

            if (!tryParseUint64Arg(argv[i + 3], eventId, "eventId"))
                return;
            if (!tryParseUint64Arg(argv[i + 4], option, "option"))
                return;
            if (!tryParseUint64Arg(argv[i + 5], amount, "amount"))
                return;
            if (!tryParseUint64Arg(argv[i + 6], price, "price"))
                return;

            if (strcmp(action, "add") == 0 && strcmp(side, "ask") == 0)
            {
                qtryAddToAskOrder(nodeIp, nodePort, seed, eventId, option, amount,
                    static_cast<int64_t>(price), defaultAntiSpamAmount, scheduledTickOffset);
            }
            else if (strcmp(action, "add") == 0 && strcmp(side, "bid") == 0)
            {
                qtryAddToBidOrder(nodeIp, nodePort, seed, eventId, option, amount,
                    static_cast<int64_t>(price), defaultAntiSpamAmount, scheduledTickOffset);
            }
            else if (strcmp(action, "remove") == 0 && strcmp(side, "ask") == 0)
            {
                qtryRemoveAskOrder(nodeIp, nodePort, seed, eventId, option, amount,
                    static_cast<int64_t>(price), defaultAntiSpamAmount, scheduledTickOffset);
            }
            else if (strcmp(action, "remove") == 0 && strcmp(side, "bid") == 0)
            {
                qtryRemoveBidOrder(nodeIp, nodePort, seed, eventId, option, amount,
                    static_cast<int64_t>(price), defaultAntiSpamAmount, scheduledTickOffset);
            }
            else
            {
                LOG("Invalid qtryorder command: %s %s. Expected: add/remove bid/ask\n", action, side);
            }
            return;
        }

        if (strcmp(argv[i], "getorder") == 0)
        {
            if (!hasRequiredParameters(i, argc, 4, "getorder"))
                return;

            const char* side = argv[i + 1];
            uint64_t eventId = 0;
            uint64_t option = 0;
            uint64_t offset = 0;

            if (!tryParseUint64Arg(argv[i + 2], eventId, "eventId"))
                return;
            if (!tryParseUint64Arg(argv[i + 3], option, "option"))
                return;
            if (!tryParseUint64Arg(argv[i + 4], offset, "offset"))
                return;

            const uint64_t isBid = (strcmp(side, "bid") == 0) ? 1 : 0;
            if (strcmp(side, "bid") != 0 && strcmp(side, "ask") != 0)
            {
                LOG("Invalid qtrygetorder side: %s. Expected: bid/ask\n", side);
                return;
            }

            quotteryPrintOrders(nodeIp, nodePort, eventId, option, isBid, offset);
            return;
        }

        if (strcmp(argv[i], "getposition") == 0)
        {
            if (!hasRequiredParameters(i, argc, 1, "getposition"))
                return;

            const char* identity = argv[i + 1];

            quotteryPrintUserPosition(nodeIp, nodePort, identity);
            return;
        }

        if (strcmp(argv[i], "geteventinfo") == 0)
        {
            if (!hasRequiredParameters(i, argc, 1, "geteventinfo"))
                return;

            uint64_t eventId = 0;
            if (!tryParseUint64Arg(argv[i + 1], eventId, "eventId"))
                return;

            quotteryPrintEventInfo(nodeIp, nodePort, eventId);
            return;
        }

        if (strcmp(argv[i], "geteventinfobatch") == 0)
        {
            if (!hasAtLeastRequiredParameters(i, argc, 1, "geteventinfobatch"))
                return;

            uint64_t eventIds[64] = {};
            size_t count = 0;
            int j = i + 1;

            while (j < argc && count < 64)
            {
                if (!tryParseUint64Arg(argv[j], eventIds[count], "eventId"))
                    return;
                ++count;
                ++j;
            }

            if (j < argc)
            {
                LOG("Error: geteventinfobatch supports at most 64 event ids\n");
                return;
            }

            quotteryPrintEventInfoBatch(nodeIp, nodePort, eventIds, count);
            return;
        }

        if (strcmp(argv[i], "geteventgroup") == 0)
        {
            if (!hasRequiredParameters(i, argc, 1, "geteventgroup"))
                return;

            uint64_t eventGroupId = 0;
            if (!tryParseUint64Arg(argv[i + 1], eventGroupId, "eventGroupId"))
                return;

            quotteryPrintEventGroup(nodeIp, nodePort, eventGroupId);
            return;
        }

        if (strcmp(argv[i], "getmarketeventgroup") == 0)
        {
            if (!hasRequiredParameters(i, argc, 1, "getmarketeventgroup"))
                return;

            uint64_t marketId = 0;
            if (!tryParseUint64Arg(argv[i + 1], marketId, "marketId"))
                return;

            quotteryPrintMarketEventGroup(nodeIp, nodePort, marketId);
            return;
        }

        if (strcmp(argv[i], "geteventgroupinfobatch") == 0)
        {
            if (!hasAtLeastRequiredParameters(i, argc, 1, "geteventgroupinfobatch"))
                return;

            uint64_t eventGroupIds[64] = {};
            size_t count = 0;
            int j = i + 1;
            while (j < argc && count < 64)
            {
                if (!tryParseUint64Arg(argv[j], eventGroupIds[count], "eventGroupId"))
                    return;
                ++count;
                ++j;
            }
            if (j < argc)
            {
                LOG("Error: geteventgroupinfobatch supports at most 64 event group ids\n");
                return;
            }

            quotteryPrintEventGroupInfoBatch(nodeIp, nodePort, eventGroupIds, count);
            return;
        }

        if (strcmp(argv[i], "publisheventresult") == 0)
        {
            if (!hasRequiredParameters(i, argc, 2, "publisheventresult"))
                return;

            uint64_t eventGroupId = 0;
            uint64_t winningMarketId = 0;
            if (!tryParseUint64Arg(argv[i + 1], eventGroupId, "eventGroupId") ||
                !tryParseUint64Arg(argv[i + 2], winningMarketId, "winningMarketId"))
                return;

            qtryPublishEventResult(nodeIp, nodePort, seed, scheduledTickOffset,
                eventGroupId, winningMarketId);
            return;
        }

        if (strcmp(argv[i], "disputeeventresult") == 0)
        {
            if (!hasRequiredParameters(i, argc, 1, "disputeeventresult"))
                return;

            uint64_t eventGroupId = 0;
            if (!tryParseUint64Arg(argv[i + 1], eventGroupId, "eventGroupId"))
                return;

            qtryDisputeEventResult(nodeIp, nodePort, seed, scheduledTickOffset, eventGroupId);
            return;
        }

        if (strcmp(argv[i], "resolveeventdispute") == 0)
        {
            if (!hasRequiredParameters(i, argc, 2, "resolveeventdispute"))
                return;

            uint64_t eventGroupId = 0;
            uint64_t winningMarketId = 0;
            if (!tryParseUint64Arg(argv[i + 1], eventGroupId, "eventGroupId") ||
                !tryParseUint64Arg(argv[i + 2], winningMarketId, "winningMarketId"))
                return;

            qtryResolveEventDispute(nodeIp, nodePort, seed, scheduledTickOffset,
                eventGroupId, winningMarketId);
            return;
        }

        if (strcmp(argv[i], "publishresult") == 0) {
            if (!hasRequiredParameters(i, argc, 2, "publishresult"))
                return;

            uint64_t eventId = 0;
            uint64_t optionId = 0;
            if (!tryParseUint64Arg(argv[i + 1], eventId, "eventId"))
                return;
            if (!tryParseUint64Arg(argv[i + 2], optionId, "optionId"))
                return;

            qtryPublishResult(nodeIp, nodePort, seed, scheduledTickOffset, eventId, optionId);
            return;
        }

        if (strcmp(argv[i], "tryfinalizeevent") == 0)
        {
            if (!hasRequiredParameters(i, argc, 1, "tryfinalizeevent"))
                return;

            uint64_t eventId = 0;
            if (!tryParseUint64Arg(argv[i + 1], eventId, "eventId"))
                return;

            qtryTryFinalizeEvent(nodeIp, nodePort, seed, scheduledTickOffset, eventId);
            return;
        }

        if (strcmp(argv[i], "dispute") == 0)
        {
            if (!hasRequiredParameters(i, argc, 1, "dispute"))
                return;

            uint64_t eventId = 0;
            if (!tryParseUint64Arg(argv[i + 1], eventId, "eventId"))
                return;

            qtryDispute(nodeIp, nodePort, seed, scheduledTickOffset, eventId);
            return;
        }

        if (strcmp(argv[i], "resolvedispute") == 0)
        {
            if (!hasRequiredParameters(i, argc, 2, "resolvedispute"))
                return;

            uint64_t eventId = 0;
            uint64_t voteRaw = 0;
            if (!tryParseUint64Arg(argv[i + 1], eventId, "eventId"))
                return;
            if (!tryParseUint64Arg(argv[i + 2], voteRaw, "vote"))
                return;

            if (voteRaw != 0 && voteRaw != 1)
            {
                LOG("Error: vote must be 0 (No) or 1 (Yes)\n");
                return;
            }

            qtryResolveDispute(nodeIp, nodePort, seed, scheduledTickOffset, eventId, static_cast<int64_t>(voteRaw));
            return;
        }

        if (strcmp(argv[i], "claimreward") == 0)
        {
            if (!hasRequiredParameters(i, argc, 1, "claimreward"))
                return;

            uint64_t eventId = 0;
            if (!tryParseUint64Arg(argv[i + 1], eventId, "eventId"))
                return;

            qtryUserClaimReward(nodeIp, nodePort, seed, scheduledTickOffset, eventId);
            return;
        }

        if (strcmp(argv[i], "forceclaimreward") == 0)
        {
            // Usage: forceclaimreward <eventId> <identity1> [identity2] ... [identity16]
            if (!hasAtLeastRequiredParameters(i, argc, 2, "forceclaimreward"))
                return;

            uint64_t eventId = 0;
            if (!tryParseUint64Arg(argv[i + 1], eventId, "eventId"))
                return;

            const char* identities[16] = {};
            int identityCount = 0;
            int j = i + 2;

            while (j < argc && identityCount < 16)
            {
                identities[identityCount] = argv[j];
                ++identityCount;
                ++j;
            }

            if (j < argc)
            {
                LOG("Error: forceclaimreward supports at most 16 identities\n");
                return;
            }

            if (identityCount == 0)
            {
                LOG("Error: forceclaimreward requires at least one identity\n");
                return;
            }

            qtryGOForceClaimReward(nodeIp, nodePort, seed, scheduledTickOffset,
                eventId, identities, identityCount);
            return;
        }

        if (strcmp(argv[i], "transfersharemgmt") == 0)
        {
            // Usage: transfersharemgmt <issuerIdentity> <assetName> <numberOfShares> <newManagingContractIndex>
            if (!hasRequiredParameters(i, argc, 4, "transfersharemgmt"))
                return;

            const char* issuerIdentity = argv[i + 1];
            const char* assetName = argv[i + 2];

            uint64_t sharesRaw = 0;
            if (!tryParseUint64Arg(argv[i + 3], sharesRaw, "numberOfShares"))
                return;

            uint64_t contractIndexRaw = 0;
            if (!tryParseUint64Arg(argv[i + 4], contractIndexRaw, "newManagingContractIndex"))
                return;

            qtryTransferShareManagementRights(nodeIp, nodePort, seed, scheduledTickOffset,
                issuerIdentity, assetName,
                static_cast<int64_t>(sharesRaw),
                static_cast<uint32_t>(contractIndexRaw));
            return;
        }

        if (strcmp(argv[i], "cleanmemory") == 0)
        {
            if (!hasRequiredParameters(i, argc, 0, "cleanmemory"))
                return;

            qtryCleanMemory(nodeIp, nodePort, seed, scheduledTickOffset);
            return;
        }

        if (strcmp(argv[i], "transferqtrygov") == 0)
        {
            if (!hasRequiredParameters(i, argc, 2, "transferqtrygov"))
                return;

            const char* receiverIdentity = argv[i + 1];
            uint64_t amount = 0;
            if (!tryParseUint64Arg(argv[i + 2], amount, "amount"))
                return;

            qtryTransferQTRYGOV(nodeIp, nodePort, seed, receiverIdentity,
                static_cast<int64_t>(amount), scheduledTickOffset);
            return;
        }

        if (strcmp(argv[i], "updatediscount") == 0)
        {
            // Usage: updatediscount <userIdentity> <set|remove> [newFeeRate]
            if (!hasAtLeastRequiredParameters(i, argc, 2, "updatediscount"))
                return;

            const char* userIdentity = argv[i + 1];
            const char* action = argv[i + 2];

            if (strcmp(action, "remove") == 0)
            {
                if (!hasRequiredParameters(i, argc, 2, "updatediscount"))
                    return;
                qtryUpdateFeeDiscountList(nodeIp, nodePort, seed, scheduledTickOffset,
                    userIdentity, 0, 0);
            }
            else if (strcmp(action, "set") == 0)
            {
                if (!hasRequiredParameters(i, argc, 3, "updatediscount set"))
                    return;
                uint64_t newFeeRate = 0;
                if (!tryParseUint64Arg(argv[i + 3], newFeeRate, "newFeeRate"))
                    return;
                qtryUpdateFeeDiscountList(nodeIp, nodePort, seed, scheduledTickOffset,
                    userIdentity, newFeeRate, 1);
            }
            else
            {
                LOG("Error: updatediscount action must be 'set' or 'remove'\n");
                return;
            }
            return;
        }

        if (strcmp(argv[i], "proposalvote") == 0)
        {
            // Usage: proposalvote <operationFee> <shareholderFee> <burnFee> <feePerDay> <depositAmountForDispute> <operationIdIdentity>
            if (!hasRequiredParameters(i, argc, 6, "proposalvote"))
                return;

            uint64_t opFee = 0, shFee = 0, burnFee = 0;
            uint64_t feePerDayRaw = 0, depositRaw = 0;
            if (!tryParseUint64Arg(argv[i + 1], opFee, "operationFee")) return;
            if (!tryParseUint64Arg(argv[i + 2], shFee, "shareholderFee")) return;
            if (!tryParseUint64Arg(argv[i + 3], burnFee, "burnFee")) return;
            if (!tryParseUint64Arg(argv[i + 4], feePerDayRaw, "feePerDay")) return;
            if (!tryParseUint64Arg(argv[i + 5], depositRaw, "depositAmountForDispute")) return;
            const char* opIdIdentity = argv[i + 6];

            qtryProposalVote(nodeIp, nodePort, seed, scheduledTickOffset,
                opFee, shFee, burnFee,
                static_cast<int64_t>(feePerDayRaw),
                static_cast<int64_t>(depositRaw),
                opIdIdentity);
            return;
        }

        if (strcmp(argv[i], "getapprovedamount") == 0)
        {
            if (!hasRequiredParameters(i, argc, 1, "getapprovedamount"))
                return;

            const char* identity = argv[i + 1];

            quotteryGetApprovedAmount(nodeIp, nodePort, identity);
            return;
        }

        if (strcmp(argv[i], "gettopproposals") == 0)
        {
            if (!hasRequiredParameters(i, argc, 0, "gettopproposals"))
                return;

            quotteryGetTopProposals(nodeIp, nodePort);
            return;
        }

        if (strcmp(argv[i], "transferqusd") == 0)
        {
            if (!hasRequiredParameters(i, argc, 2, "transferqusd"))
                return;

            const char* receiverIdentity = argv[i + 1];
            uint64_t amount = 0;
            if (!tryParseUint64Arg(argv[i + 2], amount, "amount"))
                return;

            qtryTransferQUSD(nodeIp, nodePort, seed, receiverIdentity,
                static_cast<int64_t>(amount), scheduledTickOffset);
            return;
        }

        ++i;
    }

    LOG("Error: no valid Quottery command provided\n");
}
