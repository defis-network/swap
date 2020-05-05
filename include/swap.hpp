#include <types.hpp>
#include <utils.hpp>
#include <math.h>

CONTRACT swap : public contract
{
public:
   using contract::contract;

   static constexpr uint64_t TRADE_FEE = 2; //  (2 / 1000)
   static constexpr uint64_t MINIMUM_LIQUIDITY = 1000; 
   static constexpr uint64_t PRICE_BASE = 10000; 

   swap(name receiver, name code, datastream<const char *> ds)
       : contract(receiver, code, ds),
         _markets(_self, _self.value),
         _orders(_self, _self.value),
         _globals(_self, _self.value)
   {
   }

   /**
    * newmarket action.
    *
    * @details Allows any user to create a token exchange market .
 
    * @param contract0 - the account that creates the token,
    * @param contract1 - the account that creates the token,
    * @param sym0 - the account that creates the token,
    * @param sym1 - the maximum supply set for the token created.
    *
    * @pre contract0 has to be valid,
    * @pre contract1  has to be valid,
    * @pre sym0 has to be valid,
    * @pre sym1 has to be valid.
    *
    * If validation is successful a new exchange market for tokenA and tokenB gets created.
    */
   ACTION newmarket(name creator, name contract0, name contract1, symbol sym0, symbol sym1);

   ACTION rmmarket(uint64_t mid);

   ACTION deposit(name user, uint64_t mid);

   ACTION cancle(name user);

   ACTION withdraw(name contract0, name contract1, symbol sym0, symbol sym1);

   void handle_transfer(name from, name to, asset quantity, std::string memo, name code);

   static asset get_supply(const name &token_contract_account, const symbol_code &sym_code)
   {
      stats statstable(token_contract_account, sym_code.raw());
      std::string err_msg = "invalid token contract: ";
      err_msg.append(token_contract_account.to_string());
      const auto &st = statstable.require_find(sym_code.raw(), err_msg.c_str());
      return st->supply;
   }

   static asset get_balance(const name &token_contract_account, const name &owner, const symbol_code &sym_code)
   {
      accounts accountstable(token_contract_account, owner.value);
      const auto &ac = accountstable.get(sym_code.raw());
      return ac.balance;
   }

private:
   TABLE market
   {
      uint64_t mid;

      name contract0;
      name contract1;

      symbol sym0;
      symbol sym1;

      asset reserve0;
      asset reserve1;

      uint64_t liquidity_token;

      double price0_last; // the last price is easy to controll, just for kline display, don't use in other dapp directly
      double price1_last;

      uint64_t price0_cumulative_last;
      uint64_t price1_cumulative_last;

      time_point_sec last_update;

      uint64_t primary_key() const { return mid; }

      EOSLIB_SERIALIZE(market, (mid)(contract0)(contract1)(sym0)(sym1)(reserve0)(reserve1)(liquidity_token)(price0_last)(price1_last)(price0_cumulative_last)(price0_cumulative_last)(last_update))
   };

   TABLE order
   {
      name owner;
      uint64_t mid;
      asset quantity0;
      asset quantity1;

      uint64_t primary_key() const { return owner.value; }
      EOSLIB_SERIALIZE(order, (owner)(mid)(quantity0)(quantity1))
   };

   TABLE liquidity
   {
      name owner;
      uint64_t token;

      uint64_t primary_key() const { return owner.value; }
      EOSLIB_SERIALIZE(liquidity, (owner)(token))
   };

   TABLE globals
   {
      uint64_t market_id;
      EOSLIB_SERIALIZE(globals, (market_id))
   };

   typedef multi_index<"markets"_n, market> markets;
   markets _markets;
   typedef multi_index<"orders"_n, order> orders;
   orders _orders;
   typedef eosio::singleton<"globals"_n, globals> globals_index;
   globals_index _globals;
   typedef multi_index<"liquidity"_n, liquidity> liquidity_index;

   void do_deposit(uint64_t mid, name from, asset quantity, name code);
   void add_liquidity(name user);
   void mint_liquidity_token(uint64_t mid, name to, uint64_t amount);
   void do_swap(uint64_t mid, name from, asset quantity, name code);
   void update(uint64_t mid, uint64_t balance0, uint64_t balance1, uint64_t reserve0, uint64_t reserve1);
   uint64_t get_mid();
   uint64_t quote(uint64_t amount0, uint64_t reserve0, uint64_t reserve1);
};

