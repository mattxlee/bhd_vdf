#include "generic.h"

namespace generic
{

string getstream(istream& targ, int block_size, string* buffer) {
    string new_buffer;
    string& res=(buffer!=nullptr)? *buffer : new_buffer;
    res.clear();
    
    while(1) {
        res.resize(res.size()+block_size);
        targ.read(&(res[res.size()-block_size]), block_size);
        int c=targ.gcount();
        if (c!=block_size) {
            res.resize(res.size()-block_size+c);
            assert(targ.eof());
            return new_buffer;
        }
    }
}

string getfile(const string& name, bool binary, int block_size) {
    ifstream in(name, binary? ios::binary|ios_base::in : ios_base::in);
    assert(in.good());
    return getstream(in, block_size);
}

void str_impl(vector<string>& out) {}

template<> std::ostream& print_as_number<unsigned char>(std::ostream& out, const unsigned char& targ) { out << int(targ); return out; }
template<> std::ostream& print_as_number<signed char>(std::ostream& out, const signed char& targ) { out << int(targ); return out; }
template<> std::ostream& print_as_number<char>(std::ostream& out, const char& targ) { out << int(targ); return out; }

} // namespace generic
