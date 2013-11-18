/*
*  A subfilter of banjax that publishes all log information for each
*  request in a zmq socket so botbanger-python can grab them and 
*  does svm computation with them
*
*  Vmon: Oct 2013: Initial version.
*/
#include <string>
#include <list>
#include <vector>
#include <ctime>

#include <stdio.h>
#include <string.h>
#include <zmq.hpp>

#include <re2/re2.h> //google re2

#include <ts/ts.h>

using namespace std;

#include "banjax_common.h"
#include "logentry.h"
#include "util.h"
#include "bot_sniffer.h"
#include "ip_database.h" 

/**
  Reads botbanger's port from the config
 */
void
BotSniffer::load_config(libconfig::Setting& cfg)
{
   try
   {
     botbanger_port = cfg["botbanger_port"];
     
   }
   catch(const libconfig::SettingNotFoundException &nfex)
     {
       // Ignore.
     }

   TSDebug(BANJAX_PLUGIN_NAME, "Connecting to botbanger server...");
   zmqsock.bind(("tcp://"+botbanger_server +":"+to_string(botbanger_port)).c_str());
 
}

FilterResponse BotSniffer::execute(const TransactionParts& transaction_parts)
{

  /*TSDebug("banjax", "sending log to botbanger");
  TSDebug("banjax", "ip = %s", cd->client_ip);
  TSDebug("banjax", "url = %s", cd->url);
  TSDebug("banjax", "ua = %s", cd->ua);
  TSDebug("banjax", "size = %d", (int) cd->request_len);
  TSDebug("banjax", "status = %d", stat);
  TSDebug("banjax", "protocol = %s", cd->protocol);
  TSDebug("banjax", "hit = %d", cd->hit);*/

  std::time_t rawtime;
  std::time(&rawtime);
  std::tm* timeinfo = std::gmtime(&rawtime);

  char time_buffer[80];
  std::strftime(time_buffer,80,"%Y-%m-%dT%H:%M:%S",timeinfo);
  
  send_zmq_mess(zmqsock, BOTBANGER_LOG, true);

  send_zmq_mess(zmqsock, transaction_parts.at(TransactionMuncher::IP), true);
  send_zmq_mess(zmqsock, time_buffer, true);
  send_zmq_mess(zmqsock, transaction_parts.at(TransactionMuncher::URL), true);
  send_zmq_mess(zmqsock, transaction_parts.at(TransactionMuncher::PROTOCOL), true);
  send_zmq_mess(zmqsock, transaction_parts.at(TransactionMuncher::STATUS), true);
  send_zmq_mess(zmqsock, transaction_parts.at(TransactionMuncher::CONTENT_LENGTH), true);
  send_zmq_mess(zmqsock, transaction_parts.at(TransactionMuncher::UA), true);
  send_zmq_mess(zmqsock, transaction_parts.count(TransactionMuncher::MISS) ? "MISS" : "HIT");

  char* end;
  size_t size = sizeof(LogEntry);
  struct LogEntry* le = (LogEntry*)TSmalloc(size);

  // Zero the struct, so all char[]'s within it are guaranteed to be
  // 0-terminated later on.
  memset(le, 0, sizeof(LogEntry) - 1);

  strncpy(le->hostname, transaction_parts.at(TransactionMuncher::HOST).c_str(),
          sizeof(le->hostname) - 1);
  strncpy(le->url, transaction_parts.at(TransactionMuncher::URL).c_str(),
          sizeof(le->url) - 1);

  le->start = rawtime;
  le->msDuration = strtol(transaction_parts.at(TransactionMuncher::TXN_MS_DURATION).c_str(), &end, 10);
  le->httpCode = atoi(transaction_parts.at(TransactionMuncher::STATUS).c_str());
  le->payloadsize= strtol(transaction_parts.at(TransactionMuncher::CONTENT_LENGTH).c_str(), &end, 10);
  le->cacheLookupStatus = transaction_parts.count(TransactionMuncher::MISS)
      ? CacheLookupStatus::Hit : CacheLookupStatus::Miss;

  strncpy(le->useraddress, transaction_parts.at(TransactionMuncher::IP).c_str(),
          sizeof(le->useraddress) - 1);
  strncpy(le->contenttype, transaction_parts.at(TransactionMuncher::CONTENT_TYPE).c_str(),
          sizeof(le->contenttype) - 1);
  strncpy(le->useragent, transaction_parts.at(TransactionMuncher::UA).c_str(),
          sizeof(le->useragent) - 1);

  std::string message((char*)le, sizeof(LogEntry));
  // XXX(oschaaf):
  send_zmq_mess(zmqsock, message, true);
  
  //botbanger_interface.add_log(transaction_parts[IP], cd->url, cd->protocol, stat, (long) cd->request_len, cd->ua, cd->hit);
  //botbanger_interface.add_log(cd->client_ip, time_str, cd->url, protocol, status, size, cd->ua, hit);
  return FilterResponse(FilterResponse::GO_AHEAD_NO_COMMENT);
                    
}
