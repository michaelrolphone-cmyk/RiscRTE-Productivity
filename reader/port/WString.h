#pragma once
#include <string>
class String : public std::string {public: using std::string::string; explicit String(const std::string& s):std::string(s){};};
