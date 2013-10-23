/**
   This class is responsible to store global states for each requester ip. In particular 
   it is supposed:
   - Memory allocation.
   - Take care of locking and unlocking.
   - Keeping track of memory limit and garbage collection.

   AUTHORS:
   - Vmon: Oct 2013: Initial version
*/

#include "ip_database.h"
#include "banjax.h"

using namespace std;
/**
  check if  the ip is in the db, if not store it and updates its states
  related to that filter

  returns true on success and false on failure (locking not possible)
  eventually the continuation need to be retried
*/
bool IPDatabase::set_ip_state(std::string& ip, FilterIDType filter_id, FilterState state)
{
  if (TSMutexLockTry(db_mutex) != TS_SUCCESS) {
    TSDebug(Banjax::BANJAX_PLUGIN_NAME.c_str(), "Unable to get lock on the ip db");
    return false;
  }

  IPHashTable::iterator cur_ip_it = _ip_db.find(ip);
  if (cur_ip_it == _ip_db.end()) {
    _ip_db[ip] = IPState();
    for(unsigned int i = 0; i < NUMBER_OF_STATE_KEEPER_FILTERS; i++)
      _ip_db[ip].state_array[i] = FilterState();

  }

  _ip_db[ip].state_array[filter_to_column[filter_id]]  = state;

  TSMutexUnlock(db_mutex);
  return true;

}

/**
  check if  the ip is in the db, if return 0
  otherwise return the current state
*/
FilterState IPDatabase::get_ip_state(std::string& ip, FilterIDType filter_id)
{
    IPHashTable::iterator cur_ip_it = _ip_db.find(ip);
    if (cur_ip_it == _ip_db.end())
      return FilterState();

    return cur_ip_it->second.state_array[filter_to_column[filter_id]];

}