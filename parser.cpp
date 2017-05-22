// -lboost_timer

#include <iostream>
#include <cstdio>
#include <sstream>
#include <fstream>
#include <vector>
#include <bitset>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/assert.hpp>
#include <boost/timer/timer.hpp>

using namespace std;

typedef uint32_t IpAddr;
typedef pair<uint32_t, uint32_t> Prefix;
typedef pair<Prefix, IpAddr> FibEntry;
typedef uint32_t Block;

IpAddr ipToInt(const string& prefix)
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
        sum += (boost::lexical_cast<uint32_t>(numbers[i]) * weight);
        weight >>= 8;
    }
    return sum;
}

string intToIp(IpAddr ip)
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

vector<FibEntry> g_fib;
// 668507
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
        // std::cerr << err.what() << "\n";
        // std::cerr << "skipped: " + line << "\n";
    }
    g_fib.emplace_back(prefix, dest);
}


class MultiBitTrie
{
public:

    class Node
    {
    public:
        Node(size_t nBit,
             bool hasNextHop = false,
             IpAddr nextHop = 0,
             size_t nPartial = 0)
            : m_hasNextHop(hasNextHop)
            , m_nextHop(nextHop)
            , m_next(1 << nBit)
            , m_nPartial(0)
        {
            // the size of next[] is (1 << (nBit - 1)), i.e. 2^n
            // if there is a dest, then how many bits there are valid,
            // from left to right, is indicated in nPartial.
            // this value is used when override entry
            if (hasNextHop && nPartial == 0) {
                m_nPartial = nBit;
            } else {
                m_nPartial = nPartial;
            }
        }

        void
        setNewNextHop(IpAddr nextHop, size_t nPartial)
        {
            if (m_hasNextHop && nPartial < m_nPartial) {
                // if it's less than previous value, just ignore
                return ;
            }
            m_hasNextHop = true;
            m_nextHop = nextHop;
            m_nPartial = nPartial;
        }

    public:
        bool m_hasNextHop;
        IpAddr m_nextHop;
        vector<Node*> m_next;
        size_t m_nPartial;
    };

    MultiBitTrie(vector<int> strops)
        : m_strops(strops)
    {
        int sum = 0;
        for (const int& val : strops) {
            if (val <= 0) {
                throw std::runtime_error("strop length must be larger than 0");
            }
            sum += val;
        }
        if (sum != 32) {
            throw std::runtime_error("sum of strops is not equal to 32");
        }
        m_root = new Node(strops.front());
    }

    void insert(const Prefix& prefix, const IpAddr& dest)
    {
        // one optimization is to store Ip reversely.....
        Node* p = m_root;
        Block prefixBits = prefix.first;
        int prefixLength = prefix.second;

        int processedLength = 0;
        int currentStrop = 0;
        int nextStrop = 0;
        for (size_t i = 0; i < m_strops.size(); i++) {
            // if the last part is less than the next strop
            // break to last part process
            currentStrop = m_strops[i];
            if (i == m_strops.size() - 1) {
                nextStrop = 0;
            } else {
                nextStrop = m_strops[i + 1];
            }

            if (processedLength + currentStrop > prefixLength) {
                break;
            }

            Block val = getChunk(prefixBits, currentStrop);
            if (p->m_next[val] == nullptr) {
                p->m_next[val] = new Node(nextStrop);
            }

            processedLength += currentStrop;
            p = p->m_next[val];
            prefixBits <<= currentStrop;
        }

        if (processedLength < prefixLength) {
            int nRest = prefixLength - processedLength;
            Block rest = getChunk(prefixBits, nRest);
            Block begin = rest << (currentStrop - nRest);
            Block end = begin + (1 << (currentStrop - nRest)) - 1;
            for (Block i = begin; i <= end; i++) {
                if (p->m_next[i] == nullptr) {
                    p->m_next[i] = new Node(nextStrop);
                }
                p->m_next[i]->setNewNextHop(dest, rest);
            }
        } else {
            p->setNewNextHop(dest, currentStrop);
        }
    }

    IpAddr
    lookUp(const IpAddr& ip) const
    {
        IpAddr dest = 0;
        Node* p = m_root;
        Block prefixBits = ip;
        int currentStrop = 0;
        for (size_t i = 0; i < m_strops.size(); i++) {
            currentStrop = m_strops[i];
            Block val = getChunk(prefixBits, currentStrop);
            if (p->m_next[val] == nullptr) {
                return dest;
            }
            p = p->m_next[val];
            prefixBits <<= currentStrop;
            if (p->m_hasNextHop) {
                dest = p->m_nextHop;
            }
        }
        return dest;
    }

    Block
    getChunk(Block ip, int nBits) const
    {
        return ip >> (32 - nBits);
    }

    vector<int> m_strops;
    Node* m_root;
};

class Router
{
public:
    Router(vector<int> strops = vector<int>(32, 1))
        : m_trie(strops)
    {};

    // build the routing from fibs
    void
    build(const vector<FibEntry>& fib) {
        for (const FibEntry& entry : g_fib) {
            m_trie.insert(entry.first, entry.second);
        }
    }

    IpAddr
    lookUp(const IpAddr& ip) const
    {
        return m_trie.lookUp(ip);
    }

    MultiBitTrie m_trie;
};

void test(const Router& router)
{
    boost::timer::auto_cpu_timer t;
    ifstream fin("MillionIPAddrOutput.txt");
    ofstream fout("TestOutput.txt");
    vector<IpAddr> ips;
    string ip;
    while (!fin.eof()) {
        fin >> ip;
        ips.push_back(ipToInt(ip));
    }
    for (size_t i = 0; i < ips.size(); ++i) {
        IpAddr rst = router.lookUp(ips[i]);
        if (i < 100) {
            fout << intToIp(rst) << "\n";
        }
    }
}

// we assume that there is no same prefix that has different nexthop
int main(int argc, char *argv[])
{
    ifstream bgpFin("bgptable.txt");
    string line;
    while(getline(bgpFin, line)) {
        if (bgpFin.eof()) {
            break;
        }
        parseLine(line);
    }
    Router router({12, 8, 8, 4});
    std::cerr << "start building" << std::endl;
    router.build(g_fib);
    std::cerr << "start testing" << std::endl;
    test(router);
    return 0;
}
