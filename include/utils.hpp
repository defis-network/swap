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

} // namespace utils
