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
typedef pair<Prefix, IpAddr> FibEntry;
typedef uint32_t Block;

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
        std::cerr << err.what() << std::endl;
        std::cerr << "skipped: " + line << std::endl;
    }
    std::cout << prefix << "    " << intToIp(dest) << "\n";
    g_fib.emplace_back(prefix, dest);
    // std::cout << intToIp(dest) << std::endl;
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

    MultiBitTrie(int nBit)
        : m_nBit(nBit)
    {
        if (nBit == 0) {
            throw std::runtime_error("nbit should be larger than 0");
        }
        m_root = new Node(nBit);
    }

    void insert(const Prefix& prefix, const IpAddr& dest)
    {
        // one optimization is to store Ip reversely.....
        Node* p = m_root;
        Block prefixBits = prefix.first;

        // this is the len without counting the last component
        int len = (prefix.second) / m_nBit;
        // if (prefix.second % m_bit != 0) len ++;

        for (int i = 0; i < len; i++) {
            Block val = getChunk(prefixBits, m_nBit);
            if (p->m_next[val] == nullptr) {
                p->m_next[val] = new Node(m_nBit);
            }
            p = p->m_next[val];
            prefixBits <<= m_nBit;
        }
        if (prefix.second % m_nBit != 0) {
            int nRest = prefix.second % m_nBit;
            Block rest = getChunk(prefixBits, nRest);
            Block begin = rest << (m_nBit - nRest);
            Block end = rest + (1 << (m_nBit - nRest)) - 1;
            for (Block i = begin; i <= end; i++) {
                p->setNewNextHop(dest, rest);
            }
        } else {
            p->setNewNextHop(dest, m_nBit);
        }
    }

    IpAddr lookUp(const IpAddr& ip) {
        // this method and insert need to be updated to
        // be able to be used 12-8-8-4, or whatever schema.
        IpAddr dest = 0;
        Node* p = m_root;
        Block prefixBits = ip;
        int len = 32 / m_nBit;
        for (int i = 0; i < len; i++) {
            Block val = getChunk(prefixBits, m_nBit);
            if (p->m_next[val] == nullptr) {
                return dest;
            }
            p = p->m_next[val];
            prefixBits <<= m_nBit;
            if (p->m_hasNextHop) {
                dest = p->m_nextHop;
            }
        }
        return dest;
    }

    Block
    getChunk(Block ip, int nBits)
    {
        Block rtn = (1 << nBits) - 1;
        return (ip & reverse(rtn)) >> (32 - m_nBit);
    }

    Block
    reverse(Block x)
    {
        x = (((x & 0xaaaaaaaa) >> 1) | ((x & 0x55555555) << 1));
        x = (((x & 0xcccccccc) >> 2) | ((x & 0x33333333) << 2));
        x = (((x & 0xf0f0f0f0) >> 4) | ((x & 0x0f0f0f0f) << 4));
        x = (((x & 0xff00ff00) >> 8) | ((x & 0x00ff00ff) << 8));
        return((x >> 16) | (x << 16));
    }

    int m_nBit;
    Node* m_root;
};

class Router
{
public:
    Router()
        : m_trie(1)
    {};

    // build the routing from fibs
    void build(const vector<FibEntry>& fib) {
        for (const FibEntry& entry : g_fib) {
            m_trie.insert(entry.first, entry.second);
        }
    }

    // insert a single entry into table
    void insert(const FibEntry& entry) {

    }

    IpAddr lookUp(const IpAddr& ip) {
        return m_trie.lookUp(ip);
    }
    MultiBitTrie m_trie;
};


// we assume that there is no same prefix that has different nexthop
int main(int argc, char *argv[])
{
    string line;
    while(getline(cin, line)) {
        if (cin.eof()) {
            break;
        }
        parseLine(line);
    }
    Router router;
    router.build(g_fib);
    IpAddr ip = 0x0100F800;
    for (int i = 0; i <= 100; i++) {
        ip++;
        std::cout << intToIp(router.lookUp(ip)) << std::endl;
    }
    return 0;
}
