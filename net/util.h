#ifndef NET_UTIL_H
#define NET_UTIL_H

#include <string>
#include <vector>

namespace net {

const std::vector<char> CodificaDados(const std::string& dados);

int DecodificaTamanho(const std::vector<char>& buffer);

}  // namespace net

#endif
