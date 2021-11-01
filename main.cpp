#include <SFML/Network/Http.hpp>
#include <SFML/Network/TcpSocket.hpp>
#include <SFML/System/Time.hpp>
#include <fmt/printf.h>
#include <iostream>
#include <sstream>
#include <cstring>
#include <cassert>
#include <sys/socket.h>
#include <errno.h> //For errno - the error number
#include <netdb.h>	//hostent
#include <arpa/inet.h>

using namespace sf;

IpAddress ResolveIpFromHostname(const char *hostname){
    assert(hostname);
    assert(hostname[strlen(hostname) - 1] != '/');

    addrinfo *result = nullptr;

    if(getaddrinfo(hostname, nullptr, nullptr, &result) == 0){
        if(result){
            in_addr_t ip = reinterpret_cast<sockaddr_in*>(result->ai_addr)->sin_addr.s_addr;

            freeaddrinfo(result);

            return IpAddress(htonl(ip));
        }
    }
    return IpAddress::None;
}

class FieldTable: private std::vector<std::pair<std::string, std::string>>{
private:
    using Pair = std::pair<std::string, std::string>;
    using Super = std::vector<Pair>;

    using Iterator = Super::iterator;
    using ConstIterator = Super::const_iterator;
public:
    void Add(std::string key, std::string value){
        Super::push_back(Pair(std::move(key), std::move(value)));
    }

    void Remove(const std::string &key){
        auto it = Find(key);
        if(it != end())
            Super::erase(it);
    }

    ConstIterator Find(const std::string &key)const{
        auto it = begin();
        for(; it != end(); ++it){
            if(it->first == key)
                return it;
        }
        return it;
    }      

    bool HasField(const std::string &key)const{
        return Find(key) != end();
    }
  
    ConstIterator begin()const{
        return Super::begin();
    }

    ConstIterator end()const{
        return Super::end();
    }
};

struct HttpRequest{
    enum Method{
        Get = 0
    };

    std::string URI = "/";
    Method Method = Method::Get;
    FieldTable Fields;
    int MajorVersion = 1;
    int MinorVersion = 1;
    std::string Body;

    std::string ToString()const{
        static const char *s_MethodsTable[]={
            "GET"
        };

        std::stringstream stream;
        stream << s_MethodsTable[(int)Method] << " " << URI << " ";

        stream << "HTTP/" << MajorVersion << '.' << MinorVersion << "\r\n";

        for(const auto &field: Fields)
            stream << field.first << ": " << field.second << "\r\n";

        stream << "\r\n";

        stream << Body;

        return stream.str();
    }
};

//if something in constructor can fail, we move away

struct HttpResponce{
    enum Status{
        // 2xx: success
        Ok             = 200, ///< Most common code returned when operation was successful
        Created        = 201, ///< The resource has successfully been created
        Accepted       = 202, ///< The request has been accepted, but will be processed later by the server
        NoContent      = 204, ///< The server didn't send any data in return
        ResetContent   = 205, ///< The server informs the client that it should clear the view (form) that caused the request to be sent
        PartialContent = 206, ///< The server has sent a part of the resource, as a response to a partial GET request

        // 3xx: redirection
        MultipleChoices  = 300, ///< The requested page can be accessed from several locations
        MovedPermanently = 301, ///< The requested page has permanently moved to a new location
        MovedTemporarily = 302, ///< The requested page has temporarily moved to a new location
        NotModified      = 304, ///< For conditional requests, means the requested page hasn't changed and doesn't need to be refreshed

        // 4xx: client error
        BadRequest          = 400, ///< The server couldn't understand the request (syntax error)
        Unauthorized        = 401, ///< The requested page needs an authentication to be accessed
        Forbidden           = 403, ///< The requested page cannot be accessed at all, even with authentication
        NotFound            = 404, ///< The requested page doesn't exist
        RangeNotSatisfiable = 407, ///< The server can't satisfy the partial GET request (with a "Range" header field)

        // 5xx: server error
        InternalServerError = 500, ///< The server encountered an unexpected error
        NotImplemented      = 501, ///< The server doesn't implement a requested feature
        BadGateway          = 502, ///< The gateway server has received an error from the source server
        ServiceNotAvailable = 503, ///< The server is temporarily unavailable (overloaded, in maintenance, ...)
        GatewayTimeout      = 504, ///< The gateway server couldn't receive a response from the source server
        VersionNotSupported = 505, ///< The server doesn't support the requested HTTP version

        // 10xx: SFML custom codes
        InvalidResponse  = 1000, ///< Response is not a valid HTTP one
        ConnectionFailed = 1001  ///< Connection with server failed
    };

    FieldTable   Fields;
    Status       Status = Status::ConnectionFailed;
    unsigned int MajorVersion = 0;
    unsigned int MinorVersion = 0;
    std::string  Body;

    static HttpResponce Parse(const std::string &responce){
        HttpResponce result;
        int status;
        std::sscanf(responce.c_str(), "HTTP/%d.%d %d\r\n", &result.MajorVersion, &result.MinorVersion, &status);
        result.Status = (enum HttpResponce::Status)status;

        std::stringstream stream(responce);
        std::string buffer;
        std::getline(stream, buffer, '\n');
        return result;
    }
};

struct HttpClient{
    TcpSocket Socket;

    Socket::Status Connect(const char *hostname, int port){
        return Socket.connect(ResolveIpFromHostname(hostname), port);
    }

    void Disconnect(){
        Socket.disconnect();
    }

    std::string SendRequest(const HttpRequest &request){
        std::string http_request = request.ToString();
        if(Socket.send(http_request.c_str(), http_request.size()) == Socket::Done){
            std::string received;

            char buffer[1024];
            size_t size;

            while(Socket.receive(buffer, sizeof(buffer), size) == Socket::Done)
                received.append(buffer, buffer + size);

            return received;
        }
        return {};
    }

};


int main(){
    const char *uri = "/http-basics";
    const char *hostname = "www.steves-internet-guide.com";

#if 1
    //std::cout << ResolveIpFromHostname("google.com") << std::endl;
    HttpClient client;
    client.Connect(hostname, 80);
    {
        HttpRequest req;
        req.URI = uri;
        req.Method = HttpRequest::Get;
        req.Fields.Add("Connection", "keep-alive");
        req.Fields.Add("User-Agent", "violet");
        req.Fields.Add("Host", "violet");
        req.Fields.Add("Host", hostname);
        req.Fields.Add("Content-Length", std::to_string(req.Body.size()));

        auto res = client.SendRequest(req);

        fmt::print("Result:\n{}", res);
    }
    client.Disconnect();
#else
    Http http("www.steves-internet-guide.com", 80);
    Http::Request req;
    req.setMethod(sf::Http::Request::Get);
    req.setUri(uri);
    req.setHttpVersion(1, 1); // HTTP 1.1
    auto res = http.sendRequest(req);

    //std::cout << res.getBody() << std::endl;
#endif
}