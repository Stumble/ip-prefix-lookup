#include <iostream>
#include <cstdio>
#include <sstream>
#include <vector>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/assert.hpp>

using namespace std;

typedef uint32_t IpAddr;
typedef pair<uint32_t, uint32_t> Prefix;

uint32_t ipToInt(const string& prefix)
{
    vector<string> numbers;
    boost::split(numbers, prefix, boost::is_any_of("."));
    if (numbers.size() != 4) {
        throw std::runtime_error("bad prefix format in " + prefix);
    }
    uint32_t sum = 0;
    uint32_t weight = 1;
    weight <<= 24;
    for (int i = 0; i < 4; i++) {
        // try {
        //     sum += (boost::lexical_cast<uint32_t>(numbers[i]) * weight);
        // } catch (std::exception e) {
        //     std::cout << "wrong prefix:" << prefix << std::endl;
        //     std::cout << e.what() << std::endl;
        // }
        sum += (boost::lexical_cast<uint32_t>(numbers[i]) * weight);
        weight >>= 8;
    }
    return sum;
}

string intToIp(uint32_t ip)
{
    string ans;
    uint32_t weight = (1 << 24);
    for (int i = 0; i < 4; i++) {
        int v = ip / weight;
        ip %= weight;
        weight >>= 8;
        ans += boost::lexical_cast<string>(v);
        if (i != 3) ans += ".";
    }
    return ans;
}

Prefix getPrefix(const string& str)
{
    uint32_t ip, mask;
    bool foundSlash = false;
    for (uint32_t i = 0; i < str.length(); i++) {
        if (str[i] == '/') {
            ip = ipToInt(str.substr(0, i));
            mask = boost::lexical_cast<uint32_t>(str.substr(i + 1));
            foundSlash = true;
            break;
        }
    }
    if (!foundSlash) {
        // notice that prefix without / is assumed to be /32 here.
        // throw std::runtime_error("does not found slash in ip prefix:" + str);
        ip = ipToInt(str);
        mask = 32;
    }
    return Prefix(ip, mask);
}

std::ostream& operator<<(std::ostream& s, const Prefix& p)
{
    s << intToIp(p.first) << "/" << p.second;
    return s;
}

void parseLine(const string& line)
{
    if (line[3] == ' ') {
        return ;
    }
    stringstream ss;
    ss << line.substr(3);

    string prefixStr;
    string destIpStr;
    ss >> prefixStr >> destIpStr;

    Prefix prefix;
    IpAddr dest = 0;
    try {
        prefix = getPrefix(prefixStr);
        dest = ipToInt(destIpStr);
    } catch (std::runtime_error err) {
        std::cerr << err.what() << std::endl;
        std::cerr << "skipped: " + line << std::endl;
    }
    std::cout << prefix << "    " << intToIp(dest) << "\n";
    // std::cout << intToIp(dest) << std::endl;
}

int main(int argc, char *argv[])
{
    string line;
    while(getline(cin, line)) {
        if (cin.eof()) {
            break;
        }
        parseLine(line);
    }
    return 0;
}
