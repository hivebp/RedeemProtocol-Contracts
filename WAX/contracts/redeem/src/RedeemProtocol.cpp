#include <RedeemProtocol.hpp>

ACTION redeemprtcol::init() {
    require_auth(get_self());
    config.get_or_create(get_self(), config_s{});
}

void redeemprtcol::check_collection_auth(name collection_name, name authorized_account) {
    require_auth(authorized_account);

    auto collection_itr = collections.require_find(collection_name.value,
        "No collection with this name exists");

    check(std::find(
        collection_itr->authorized_accounts.begin(),
        collection_itr->authorized_accounts.end(),
        authorized_account
        ) != collection_itr->authorized_accounts.end(),
        "Account is not authorized"
    );
}

ACTION redeemprtcol::accept(
    name authorized_account,
    name collection_name,
    uint64_t asset_id
) {
    check_collection_auth(collection_name, authorized_account);

    assets_t own_assets = get_assets(get_self());

    auto asset_itr = own_assets.require_find(asset_id, ("Asset ID not found in contract: " + to_string(asset_id)).c_str());

    redemption_t col_redemptions = get_collection_redemptions(asset_itr->collection_name);

    auto redemption_itr = col_redemptions.require_find(asset_id, "No active Redemption for that Asset");
    
    check(redemption_itr->status == "redeemed", "Redemption has already been accepted");

    col_redemptions.modify(redemption_itr, get_self(), [&](auto& _redemption) {
        _redemption.status = "accepted";
    });
}

ACTION redeemprtcol::markpaid(
    name asset_owner,
    uint64_t asset_id
) {
    require_auth(asset_owner);

    assets_t own_assets = get_assets(get_self());

    auto asset_itr = own_assets.require_find(asset_id, ("Asset ID not found in contract: " + to_string(asset_id)).c_str());

    redemption_t col_redemptions = get_collection_redemptions(asset_itr->collection_name);

    auto redemption_itr = col_redemptions.require_find(asset_id, "No active Redemption for that Asset");

    check(redemption_itr->asset_owner == asset_owner, "This is not your redemption process.");
    
    check(redemption_itr->status == "accepted", "Redemption is not accepted yet or already being processed");

    col_redemptions.modify(redemption_itr, get_self(), [&](auto& _redemption) {
        _redemption.status = "paid";
    });
}

ACTION redeemprtcol::markshipped(
    name authorized_account,
    name collection_name,
    uint64_t asset_id
) {
    check_collection_auth(collection_name, authorized_account);

    assets_t own_assets = get_assets(get_self());

    auto asset_itr = own_assets.require_find(asset_id, ("Asset ID not found in contract: " + to_string(asset_id)).c_str());

    redemption_t col_redemptions = get_collection_redemptions(asset_itr->collection_name);

    auto redemption_itr = col_redemptions.require_find(asset_id, "No active Redemption for that Asset");
    
    check(redemption_itr->status == "paid", "Redemption is not paid or has already been processed");

    col_redemptions.modify(redemption_itr, get_self(), [&](auto& _redemption) {
        _redemption.status = "shipped";
    });
}

ACTION redeemprtcol::markreceived(
    name asset_owner,
    uint64_t asset_id
) {
    require_auth(asset_owner);

    assets_t own_assets = get_assets(get_self());

    auto asset_itr = own_assets.require_find(asset_id, ("Asset ID not found in contract: " + to_string(asset_id)).c_str());

    redemption_t col_redemptions = get_collection_redemptions(asset_itr->collection_name);

    auto redemption_itr = col_redemptions.require_find(asset_id, "No active Redemption for that Asset");

    check(redemption_itr->asset_owner == asset_owner, "This is not your redemption process.");

    check(redemption_itr->status == "shipped", "Redemption is not shipped yet or already being processed");

    col_redemptions.modify(redemption_itr, get_self(), [&](auto& _redemption) {
        _redemption.status = "received";
    });
}

ACTION redeemprtcol::settr(name new_token_receiver) {
    require_auth(get_self());
    config_s current_config = config.get();
    current_config.token_receiver = new_token_receiver;
    config.set(current_config, get_self());
}

ACTION redeemprtcol::logredeem(
    uint64_t asset_id,
    uint64_t redemption_id,
    name collection_name
) {
    require_auth(get_self());
}

void redeemprtcol::receive_asset_transfer(
    name from,
    name to,
    vector<uint64_t> asset_ids,
    string memo
) {
    if (to != get_self()) {
        return;
    }

    check(memo.find("redeem") == 0, "Invalid Memo.");

    assets_t own_assets = get_assets(get_self());

    for (uint64_t asset_id : asset_ids) {
        auto asset_itr = own_assets.require_find(asset_id, ("Asset ID not found in contract: " + to_string(asset_id)).c_str());

        schemas_t collection_schemas = get_schemas(asset_itr->collection_name);
        auto schema_itr = collection_schemas.find(asset_itr->schema_name.value);

        ATTRIBUTE_MAP deserialized_immutable_data = deserialize(
            asset_itr->immutable_serialized_data,
            schema_itr->format
        );

        check(deserialized_immutable_data.find("redemption_type") != deserialized_immutable_data.end(),
            ("Redemption Type not found in Asset Attributes" + to_string(asset_id)).c_str());

        ATTRIBUTE_MAP deserialized_mutable_data = deserialize(
            asset_itr->mutable_serialized_data,
            schema_itr->format
        );

        check(deserialized_immutable_data.find("redemption_status") != deserialized_immutable_data.end(),
            ("Redemption Status not found in Asset Attributes for Asset " + to_string(asset_id)).c_str());

        auto redemption_itr = redemptions.find(asset_id);
        check(redemption_itr == redemptions.end(), ("This asset is already being redeemed: " + to_string(asset_id)).c_str());
        
        config_s current_config = config.get();
        uint64_t redemption_id = current_config.redemption_counter++;
        name token_receiver = current_config.token_receiver;
        config.set(current_config, get_self());

        // Scope on Collection Name
        redemption_t col_redemptions = get_collection_redemptions(asset_itr->collection_name);
        
        col_redemptions.emplace( get_self(), [&]( auto& row ) {
            row.redemption_id = redemption_id;
            row.asset_id = asset_id;
            row.asset_owner = from;
            row.status = "redeemed";
        });

        action(
            permission_level{get_self(), name("active")},
            get_self(),
            name("logredeem"),
            make_tuple(
                asset_id,
                redemption_id,
                asset_itr->collection_name
            )
        ).send();
    }
}