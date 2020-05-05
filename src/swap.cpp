#include <swap.hpp>

ACTION swap::newmarket(name creator, name contract0, name contract1, symbol sym0, symbol sym1)
{
    require_auth(creator);

    auto supply0 = get_supply(contract0, sym0.code());
    check(supply0.amount > 0, "invalid token0");
    auto supply1 = get_supply(contract1, sym1.code());
    check(supply1.amount > 0, "invalid token1");

    _markets.emplace(creator, [&](auto &a) {
        a.mid = get_mid();
        a.contract0 = contract0;
        a.contract1 = contract1;
        a.sym0 = sym0;
        a.sym1 = sym1;
        a.reserve0.symbol = sym0;
        a.reserve1.symbol = sym1;
        a.last_update = current_time_point();
    });
}

ACTION swap::rmmarket(uint64_t mid)
{
    auto itr = _markets.require_find(mid, "Market does not exist.");

    check(
        itr->reserve0.amount == 0 &&
            itr->reserve1.amount == 0 &&
            itr->liquidity_token == 0,
        "Unable to remove active market.");

    _markets.erase(itr);
}

ACTION swap::deposit(name user, uint64_t mid)
{
    require_auth(user);

    auto itr = _orders.find(user.value);

    check(itr == _orders.end(), "You have a pending order.");

    auto m_itr = _markets.require_find(mid, "Market does not exist.");

    _orders.emplace(user, [&](auto &a) {
        a.owner = user;
        a.mid = mid;
        a.quantity0.symbol = m_itr->sym0;
        a.quantity1.symbol = m_itr->sym1;
    });
}

ACTION swap::cancle(name user)
{
    require_auth(user);

    auto itr = _orders.require_find(user.value, "You don't have any order.");

    auto m_itr = _markets.require_find(itr->mid, "Market does not exist.");

    if (itr->quantity0.amount > 0)
    {
        action(
            permission_level{get_self(), "active"_n},
            m_itr->contract0,
            name("transfer"),
            make_tuple(get_self(), user, itr->quantity0, std::string("cancle order refund")))
            .send();
    }

    if (itr->quantity1.amount > 0)
    {
        action(
            permission_level{get_self(), "active"_n},
            m_itr->contract1,
            name("transfer"),
            make_tuple(get_self(), user, itr->quantity1, std::string("cancle order refund")))
            .send();
    }

    _orders.erase(itr);
}

ACTION swap::withdraw(name contract0, name contract1, symbol sym0, symbol sym1)
{
    // TODO
}

void swap::handle_transfer(name from, name to, asset quantity, std::string memo, name code)
{
    if (from == _self || to != _self)
    {
        return;
    }

    std::string act;
    uint64_t mid = 0;
    utils::parse_memo(memo, &act, &mid);

    if (act == "deposit")
    {
        do_deposit(mid, from, quantity, code);
    }

    if (act == "swap")
    {
        do_swap(mid, from, quantity, code);
    }
}

void swap::do_deposit(uint64_t mid, name from, asset quantity, name code)
{
    auto itr = _orders.require_find(from.value, "You don't have any order.");
    auto m_itr = _markets.require_find(itr->mid, "Market does not exist.");

    _orders.modify(itr, same_payer, [&](auto &s) {
        if (code == m_itr->contract0 && quantity.symbol == m_itr->sym0)
        {
            s.quantity0 += quantity;
        }

        if (code == m_itr->contract1 && quantity.symbol == m_itr->sym1)
        {
            s.quantity1 += quantity;
        }
    });

    if (itr->quantity0.amount > 0 && itr->quantity1.amount > 0)
    {
        add_liquidity(itr->owner);
    }
}

void swap::add_liquidity(name user)
{
    auto itr = _orders.require_find(user.value, "You don't have any order.");
    auto m_itr = _markets.require_find(itr->mid, "Market does not exist.");

    // step1: get amount0 and amount1
    uint64_t amount0_desired = itr->quantity0.amount;
    uint64_t amount1_desired = itr->quantity1.amount;

    uint64_t amount0 = 0;
    uint64_t amount1 = 0;

    uint64_t reserve0 = m_itr->reserve0.amount;
    uint64_t reserve1 = m_itr->reserve1.amount;

    uint64_t refund_amount0 = 0;
    uint64_t refund_amount1 = 0;

    if (reserve0 == 0 && reserve1 == 0)
    {
        amount0 = amount0_desired;
        amount1 = amount1_desired;
    }
    else
    {
        uint64_t amount1_optimal = quote(amount0_desired, reserve0, reserve1);

        if (amount1_optimal <= amount1_desired)
        {
            amount0 = amount0_desired;
            amount1 = amount1_optimal;

            refund_amount1 = amount1_desired - amount1_optimal;
        }
        else
        {
            uint64_t amount0_optimal = quote(amount1_desired, reserve1, reserve0);
            check(amount0_optimal <= amount0_desired, "math error");

            amount0 = amount0_optimal;
            amount1 = amount1_desired;

            refund_amount0 = amount0_desired - amount0_optimal;
        }
    }

    // step2: refund
    if (refund_amount0 > 0)
    {
        action(
            permission_level{get_self(), "active"_n},
            m_itr->contract0,
            name("transfer"),
            make_tuple(get_self(), user, asset(refund_amount0, m_itr->sym0), std::string("deposit refund")))
            .send();
    }

    if (refund_amount1 > 0)
    {
        action(
            permission_level{get_self(), "active"_n},
            m_itr->contract1,
            name("transfer"),
            make_tuple(get_self(), user, asset(refund_amount1, m_itr->sym1), std::string("deposit refund")))
            .send();
    }

    // TODO step3: mint liquidity token
    uint64_t token_mint = 0;

    uint64_t total_liquidity_token = m_itr->liquidity_token;

    if (total_liquidity_token == 0)
    {
        token_mint = sqrt(amount0 * amount1) - MINIMUM_LIQUIDITY;
        mint_liquidity_token(itr->mid, get_self(), MINIMUM_LIQUIDITY); // permanently lock the first MINIMUM_LIQUIDITY tokens
    }
    else
    {
        auto x = amount0 * total_liquidity_token / reserve0;
        auto y = amount1 * total_liquidity_token / reserve1;
        token_mint = std::min(amount0 * total_liquidity_token / reserve0, amount1 * total_liquidity_token / reserve1);

        print("debug: x:", x, " ,y:", y, " ,min:", token_mint);
    }

    check(token_mint > 0, "INSUFFICIENT_LIQUIDITY_MINTED");

    // step4: update user liquidity token
    mint_liquidity_token(itr->mid, user, token_mint);

    // step5: update market
    update(itr->mid, reserve0 + amount0, reserve1 + amount1, reserve0, reserve1);

    // step6: finish deposit, remove order
    _orders.erase(itr);
}

void swap::mint_liquidity_token(uint64_t mid, name to, uint64_t amount)
{
    liquidity_index liqtable(get_self(), mid);
    auto liq_itr = liqtable.find(to.value);
    if (liq_itr == liqtable.end())
    {
        liqtable.emplace(get_self(), [&](auto &a) {
            a.owner = to;
            a.token = amount;
        });
    }
    else
    {
        liqtable.modify(liq_itr, same_payer, [&](auto &a) {
            a.token += amount;
        });
    }

    auto m_itr = _markets.require_find(mid, "Market does not exist.");
    _markets.modify(m_itr, same_payer, [&](auto &a) {
        a.liquidity_token += amount;
    });
}

void swap::do_swap(uint64_t mid, name from, asset quantity, name code)
{
    print("do swap");
}

void swap::update(uint64_t mid, uint64_t balance0, uint64_t balance1, uint64_t reserve0, uint64_t reserve1)
{
    auto m_itr = _markets.require_find(mid, "Market does not exist.");

    auto last_sec = m_itr->last_update.sec_since_epoch();
    uint64_t time_elapsed = 1;
    if (last_sec > 0)
    {
        time_elapsed = current_time_point().sec_since_epoch() - last_sec;
    }

    _markets.modify(m_itr, same_payer, [&](auto &a) {
        a.reserve0.amount = balance0;
        a.reserve1.amount = balance1;

        if (time_elapsed > 0 && reserve0 != 0 && reserve1 != 0)
        {
            // * never overflows, and + overflow is desired
            auto price0 = PRICE_BASE * reserve1 / reserve0;
            auto price1 = PRICE_BASE * reserve0 / reserve1;
            print(" debug: price0: ", price0, " ,price1:", price1, " ,time_elapsed:", time_elapsed);
            a.price0_cumulative_last += price0 * time_elapsed;
            a.price1_cumulative_last += price1 * time_elapsed;

            a.price0_last = price0 / PRICE_BASE;
            a.price1_last = price1 / PRICE_BASE;
        }

        a.last_update = current_time_point();
    });
}

uint64_t swap::get_mid()
{
    globals glb = _globals.get_or_default(globals{});
    glb.market_id += 1;
    _globals.set(glb, _self);
    return glb.market_id;
}

// given some amount of an asset and pair reserves, returns an equivalent amount of the other asset
uint64_t swap::quote(uint64_t amount0, uint64_t reserve0, uint64_t reserve1)
{
    check(amount0 > 0, "INSUFFICIENT_AMOUNT0");
    check(reserve0 > 0 && reserve1 > 0, "INSUFFICIENT_LIQUIDITY");
    return amount0 * reserve1 / reserve0;
}

extern "C"
{
    void apply(uint64_t receiver, uint64_t code, uint64_t action)
    {
        auto self = receiver;

        if (code == self)
        {
            switch (action)
            {
                EOSIO_DISPATCH_HELPER(swap, (newmarket)(rmmarket)(deposit)(cancle)(withdraw))
            }
        }
        else
        {
            if (action == name("transfer").value)
            {
                swap instance(name(receiver), name(code), datastream<const char *>(nullptr, 0));
                const auto t = unpack_action_data<transfer_args>();
                instance.handle_transfer(t.from, t.to, t.quantity, t.memo, name(code));
            }
        }
    }
}