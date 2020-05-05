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

void swap::do_swap(uint64_t mid, name from, asset quantity, name code)
{
    print("do swap");
}

void swap::do_deposit(uint64_t mid, name from, asset quantity, name code)
{
    print("do deposit");
}

void swap::add_liquidity(uint64_t mid, name from, asset quantity, name code)
{
    print("add liquidity");
}

void swap::update(asset balance0, asset balance1, asset reserve0, asset reserve1)
{
}

uint64_t swap::get_mid()
{
    globals glb = _globals.get_or_default(globals{});
    glb.market_id += 1;
    _globals.set(glb, _self);
    return glb.market_id;
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