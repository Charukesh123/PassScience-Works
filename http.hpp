// Minimal blocking HTTP/1.1 server (loopback only). Transport layer; knows nothing about FEA.
#pragma once
#include <functional>
#include <map>
#include <string>
struct Request { std::string method, path, body; std::map<std::string, std::string> query; };
struct Response { int status = 200; std::string type = "application/json"; std::string body; };
using Handler = std::function<Response(const Request&)>;
// Binds 127.0.0.1 on the first free port in [first, first+20]; returns the port or 0 on failure.
int httpListen(int first);
void httpServe(Handler h);   // never returns
