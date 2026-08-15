#pragma once

#include "structs.h"
#define QUOTTERY_MAX_CONCURRENT_EVENT 4096
#define QUOTTERY_MAX_MARKETS_PER_EVENT_GROUP 64

constexpr uint8_t QUOTTERY_EVENT_GROUP_MODE_INDEPENDENT = 0;
constexpr uint8_t QUOTTERY_EVENT_GROUP_MODE_EXCLUSIVE_ONE = 1;

constexpr uint8_t QUOTTERY_EVENT_GROUP_STATUS_DRAFT = 0;
constexpr uint8_t QUOTTERY_EVENT_GROUP_STATUS_OPEN = 1;
constexpr uint8_t QUOTTERY_EVENT_GROUP_STATUS_RESOLVING = 2;
constexpr uint8_t QUOTTERY_EVENT_GROUP_STATUS_FINALIZED = 3;
struct qtryBasicInfo_output
{
    uint64_t operationFee; // 4 digit number ABCD means AB.CD% | 1234 is 12.34%
    uint64_t shareholderFee; // 4 digit number ABCD means AB.CD% | 1234 is 12.34%
    uint64_t burnFee; // percentage
    uint64_t nIssuedEvent; // number of issued event
    uint64_t shareholdersRevenue;
    uint64_t operationRevenue;
    uint64_t burnedAmount;
    uint64_t feePerDay;
    uint64_t antiSpamAmount;
    uint64_t depositAmountForDispute;
    uint8_t gameOperator[32];
    uint64_t nIssuedEventGroup;

    static constexpr unsigned char type()
    {
        return RespondContractFunction::type();
    }
};

struct getEventInfo_input
{
    uint64_t eventId;
};

struct QtryEventInfo
{
    uint64_t eid;
    uint64_t openDate; // submitted date
    uint64_t endDate; // stop receiving result from OPs
    uint8_t desc[128];
    uint8_t option0Desc[64];
    uint8_t option1Desc[64];
};

struct DepositInfo
{
    uint8_t pubkey[32];
    int64_t amount;
};

struct QtryEventGroupInfo
{
    uint64_t eventGroupId;
    uint64_t createdDate;
    uint64_t openedDate;
    uint8_t desc[128];
    uint16_t expectedMarketCount;
    uint16_t marketCount;
    uint16_t finalizedMarketCount;
    uint16_t archivedMarketCount;
    uint8_t mode;
    uint8_t status;
    uint8_t padding[6];
};

struct QtryEventGroupMarkets
{
    uint64_t marketIds[QUOTTERY_MAX_MARKETS_PER_EVENT_GROUP];
};

struct QtryMarketGroupLink
{
    uint64_t eventGroupId;
    uint16_t marketIndex;
    uint8_t padding[6];
};

struct GetEventGroup_input
{
    uint64_t eventGroupId;
};

struct GetEventGroup_output
{
    QtryEventGroupInfo eventGroupInfo;
    QtryEventGroupMarkets markets;
    int32_t winningMarketIndex;
    uint32_t padding0;
    uint64_t winningMarketId;
    uint32_t publishTickTime;
    uint32_t padding1;
    DepositInfo disputerInfo;
    uint8_t exists;
    uint8_t padding2[7];

    static constexpr unsigned char type()
    {
        return RespondContractFunction::type();
    }
};

struct GetMarketEventGroup_input
{
    uint64_t marketId;
};

struct GetMarketEventGroup_output
{
    QtryMarketGroupLink marketGroupLink;
    uint8_t mode;
    uint8_t status;
    uint8_t exists;
    uint8_t padding[5];

    static constexpr unsigned char type()
    {
        return RespondContractFunction::type();
    }
};

struct GetEventGroupInfoBatch_input
{
    uint64_t eventGroupIds[64];
};

struct GetEventGroupInfoBatch_output
{
    QtryEventGroupInfo eventGroupInfos[64];
    int8_t winningMarketIndices[64];
    uint8_t exists[64];

    static constexpr unsigned char type()
    {
        return RespondContractFunction::type();
    }
};

struct CreateEventGroup_input
{
    uint8_t desc[128];
    uint16_t expectedMarketCount;
    uint8_t mode;
    uint8_t padding[5];
};

struct AddMarket_input
{
    uint64_t eventGroupId;
    QtryEventInfo qei;
};

struct EventGroupId_input
{
    uint64_t eventGroupId;
};

struct EventGroupResult_input
{
    uint64_t eventGroupId;
    uint64_t winningMarketId;
};

static_assert(sizeof(qtryBasicInfo_output) == 120, "Unexpected Quottery BasicInfo layout");
static_assert(sizeof(QtryEventInfo) == 280, "Unexpected Quottery event layout");
static_assert(sizeof(DepositInfo) == 40, "Unexpected Quottery deposit layout");
static_assert(sizeof(QtryEventGroupInfo) == 168, "Unexpected Quottery event group layout");
static_assert(sizeof(QtryEventGroupMarkets) == 512, "Unexpected Quottery event group markets layout");
static_assert(sizeof(QtryMarketGroupLink) == 16, "Unexpected Quottery market link layout");
static_assert(sizeof(GetEventGroup_output) == 752, "Unexpected GetEventGroup output layout");
static_assert(sizeof(GetMarketEventGroup_output) == 24, "Unexpected GetMarketEventGroup output layout");
static_assert(sizeof(GetEventGroupInfoBatch_output) == 10880, "Unexpected event group batch layout");
static_assert(sizeof(CreateEventGroup_input) == 136, "Unexpected CreateEventGroup input layout");
static_assert(sizeof(AddMarket_input) == 288, "Unexpected AddMarket input layout");

struct getEventInfo_output
{
    QtryEventInfo qei;
    int32_t resultByGO; // -1 = NOT_SET, 0 = NO, 1 = YES
    uint32_t publishTickTime; // ignore if resultByGO is not set
    DepositInfo disputerInfo; // zero / null if no dispute
    uint32_t computorsVote0;
    uint32_t computorsVote1;

    static constexpr unsigned char type()
    {
        return RespondContractFunction::type();
    }
};

void unpackQuotteryDate(uint8_t& _year, uint8_t& _month, uint8_t& _day, uint8_t& _hour, uint8_t& _minute, uint8_t& _second, uint32_t data);


struct qtryGetOrders_input
{
    uint64_t eventId;
    uint64_t option;
    uint64_t isBid;
    uint64_t offset;
};

struct QtryOrder
{
    uint8_t entity[32];
    uint64_t amount;
};

struct QtryOrderWithPrice
{
    QtryOrder qo;
    int64_t price;
};

struct qtryGetOrders_output
{
    QtryOrderWithPrice orders[256];

    static constexpr unsigned char type()
    {
        return RespondContractFunction::type();
    }
};

struct PositionInfo
{
    uint64_t eo; // packed: bit 0 = option, bits 1+ = eventId
    int64_t  amount;
};

struct getUserPosition_output
{
    int64_t     count;
    PositionInfo p[1024];

    static constexpr unsigned char type()
    {
        return RespondContractFunction::type();
    }
};


struct getActiveEvent_output
{
    uint64_t recentActiveEvent[QUOTTERY_MAX_CONCURRENT_EVENT];
    static constexpr unsigned char type()
    {
        return RespondContractFunction::type();
    }
};

struct GetEventInfoBatch_input
{
    uint64_t eventIds[64];
};
struct GetEventInfoBatch_output
{
    QtryEventInfo aqei[64];
    static constexpr unsigned char type()
    {
        return RespondContractFunction::type();
    }
};


struct getApprovedAmount_input
{
    uint8_t pk[32];
};
struct getApprovedAmount_output
{
    uint64_t amount;
    static constexpr unsigned char type()
    {
        return RespondContractFunction::type();
    }
};

struct QtryGOV_cli
{
    uint64_t mShareHolderFee;
    uint64_t mBurnFee;
    uint64_t mOperationFee;
    int64_t feePerDay;
    int64_t mDepositAmountForDispute;
    uint8_t mOperationId[32];
};

struct ProposalInfo_cli
{
    QtryGOV_cli proposed;
    int64_t totalVotes;
};

struct getTopProposals_output
{
    ProposalInfo_cli top[4]; // top 3 (index 3 unused)
    int32_t uniqueCount;
    static constexpr unsigned char type()
    {
        return RespondContractFunction::type();
    }
};

void quotteryEntryPoint(int argc, char** argv, const char* nodeIp, int nodePort, const char* seed, uint32_t scheduledTickOffset);
