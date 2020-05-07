#include <types.hpp>
namespace utils
{
using std::string;

size_t sub2sep(string &input, string *output, const char &separator, const size_t &first_pos, const bool &required)
{
   check(first_pos != string::npos, "invalid first pos");
   auto pos = input.find(separator, first_pos);
   if (pos == string::npos)
   {
      check(!required, "parse memo error");
      return string::npos;
   }
   *output = input.substr(first_pos, pos - first_pos);
   return pos;
}

void parse_memo(string memo, string *action, uint64_t *id)
{
   size_t sep_count = count(memo.begin(), memo.end(), ':');

   if (sep_count == 0)
   {
      memo.erase(remove_if(memo.begin(), memo.end(), [](unsigned char x) { return isspace(x); }), memo.end());
      *action = memo;
   }
   else if (sep_count == 1)
   {
      size_t pos;
      string container;
      pos = sub2sep(memo, &container, ':', 0, true);

      *action = container;
      *id = atoi(memo.substr(++pos).c_str());
   }
}

asset get_supply(const name &token_contract_account, const symbol_code &sym_code)
{
   stats statstable(token_contract_account, sym_code.raw());
   std::string err_msg = "invalid token contract: ";
   err_msg.append(token_contract_account.to_string());
   const auto &st = statstable.require_find(sym_code.raw(), err_msg.c_str());
   return st->supply;
}

asset get_balance(const name &token_contract_account, const name &owner, const symbol_code &sym_code)
{
   accounts accountstable(token_contract_account, owner.value);
   const auto &ac = accountstable.get(sym_code.raw());
   return ac.balance;
}

void inline_transfer(name contract, name from, name to, asset quantity, string memo)
{
    action(
        permission_level{from, "active"_n},
        contract,
        name("transfer"),
        make_tuple(from, to, quantity, memo))
        .send();
}
} // namespace utils