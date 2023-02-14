#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <eosio/asset.hpp>
#include "atomicdata.hpp"

using namespace eosio;
using namespace std;
using namespace atomicdata;

CONTRACT redeemprtcol : public contract
{
public:
    using contract::contract;

    ACTION init();

    [[eosio::on_notify("atomicassets::transfer")]] void receive_asset_transfer(
        name from,
        name to,
        vector<uint64_t> asset_ids,
        string memo
    );

    ACTION settr(name new_token_receiver);

    ACTION logredeem(
        uint64_t asset_id,
        uint64_t redemption_id,
        name collection_name
    );

    ACTION accept(
        name authorized_account,
        name collection_name,
        uint64_t asset_id
    );

    ACTION markpaid(
        name asset_owner,
        uint64_t asset_id
    );

    ACTION markshipped(
        name authorized_account,
        name collection_name,
        uint64_t asset_id
    );

    ACTION markreceived(
        name asset_owner,
        uint64_t asset_id
    );
private:
    TABLE redemption_s
    {
        uint64_t asset_id;
        uint64_t redemption_id;
        name asset_owner;
        string status;

        uint64_t primary_key() const { return asset_id; }
    };
    typedef multi_index<"redemption"_n, redemption_s> redemption_t;

    // redemption_t redemptions;
    TABLE config_s
    {
        uint64_t redemption_counter = 1099511627776; // 2^40
        name token_receiver = name("waxchihkaiyu");
    };

    typedef singleton<name("config"), config_s> config_t;
    typedef multi_index<name("config"), config_s> config_t_for_abi;

    void check_collection_auth(
        name collection_name, 
        name authorized_account
    );

    config_t config = config_t(get_self(), get_self().value);

    redemption_t redemptions = redemption_t(get_self(), get_self().value);

    struct assets_s {
        uint64_t         asset_id;
        name             collection_name;
        name             schema_name;
        int32_t          template_id;
        name             ram_payer;
        vector <asset>   backed_tokens;
        vector <uint8_t> immutable_serialized_data;
        vector <uint8_t> mutable_serialized_data;

        uint64_t primary_key() const { return asset_id; };
    };

    typedef multi_index <name("assets"), assets_s> assets_t;

    struct schemas_s {
        name            schema_name;
        vector <FORMAT> format;

        uint64_t primary_key() const { return schema_name.value; }
    };

    typedef multi_index <name("schemas"), schemas_s> schemas_t;
    
    struct collections_s {
        name             collection_name;
        name             author;
        bool             allow_notify;
        vector <name>    authorized_accounts;
        vector <name>    notify_accounts;
        double           market_fee;
        vector <uint8_t> serialized_data;

        auto primary_key() const { return collection_name.value; };
    };
    
    typedef eosio::multi_index<name("collections"), collections_s> collections_t;
    
    collections_t collections = collections_t(name("atomicassets"), name("atomicassets").value);

    schemas_t get_schemas(name collection_name) {
        return schemas_t(name("atomicassets"), collection_name.value);
    }
    
    assets_t get_assets(name acc) {
        return assets_t(name("atomicassets"), acc.value);
    }
    
    redemption_t get_collection_redemptions(name collection_name) {
        return redemption_t(get_self(), collection_name.value);
    }
};