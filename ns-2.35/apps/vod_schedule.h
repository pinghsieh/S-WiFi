/*
 * vod_schedule.h
 * Author: Ping-Chun Hsieh (2015/05/20)
 */
// File: Code for a new scheduling policy for Video-on-Demand applications

// $Header: /cvsroot/nsnam/ns-2/apps/ping.h,v 1.5 2005/08/25 18:58:01 johnh Exp $


#ifndef ns_vod_schedule_h
#define ns_vod_schedule_h

#include "agent.h"
#include "tclcl.h"
#include "packet.h"
#include "address.h"
#include "ip.h"
#include "mac.h"
#include <vector>   // Not sure if vector is applicable in ns2
#include <iostream> // For debugging
#include <fstream>  // For outputting data
using std::vector;
using std::cout;
using std::endl;
using std::ofstream;


struct hdr_vod_schedule {
	char ret_;
  	//ret indicates the packet type
  	//0 for register, 1 for request, 2 for data from client to server,
  	//3 for data from server to client, 4 for register duplex traffic
  	//5 for 802.11 traffic, client to server, 6 for register downlink traffic
	//7 for user-defined ACK from client to server

    char successful_;  	// 1 for successful transmission
	double send_time_;
 	double rcv_time_;	// when packet arrived to receiver
 	int seq_;		    // sequence number
	int datasize_;      // Data size of a packet

 	//For register packet
 	double qn_;   // user demand in bytes
	int tier_;    // user priority
	int init_;    // initial data in bytes in the playback buffer
	double pn_;   // channel reliability, not in use for now	
	double wn_;   // weight factor
	int frame_;   // video frame length in slots

	// Header access methods
	static int offset_; 	// required by PacketHeaderManager
	inline static int& offset() { return offset_; }
	inline static hdr_vod_schedule* access(const Packet* p) {
		return (hdr_vod_schedule*) p->access(offset_);
	}
};

//primitive data for clients
class VoD_ScheduleClient {

public:
	VoD_ScheduleClient();
	double GetPressure_MW(int root); //TODO:
	double GetPressure_PF();         //TODO:
	double GetPressure_NOVA();       //TODO:

	u_int32_t addr_;
	bool done_;     // whether packet is delivered in this period
	double qn_;	    // throughput requirement
	int tier_;	    // tier of this client
	double pn_;     // channel reliability
	double wn_;     // weight factor
	double Xn_;     // debt for HDR. Used for other means for other policies
	int frame_;     // number of slots in a frame for periodic playback
	int countdown_; // countdown timer for periodic consumption
	vector<int> outage_history_;	// record the number of outage throughout the simulation trial
	vector<int> buffer_history_;	// record the buffer length at the beginning of each time slot
	vector<int> past_delivery_;     // record the delivered data in bits in each time slot
	bool is_active_;                // indicate whether the client is working or not
	vector<double> channel_dist_;   // specify the distribution of data rate
	double outage_;                 // total slots of outage 
	double data_stored_;            // total bits stored in the playback buffer 
	double current_rate_;           // data rate in current slot
	double data_received_;          // cumulative amounts of data received by the client
	double NOVA_index_;             // denotes the bi(t) in NOVA policy
};

class VoD_ScheduleAgent : public Agent {

public:
	VoD_ScheduleAgent();
	~VoD_ScheduleAgent();

 	int seq_;	     // a send sequence number like in real ping
	int numclient_;  // number of total clients
	//VoD_ScheduleClient* client_list_;  // For a server to handle scheduling among clients
	vector<VoD_ScheduleClient*> client_list_;  // For a server to handle scheduling among clients
	bool is_server_; // Indicate whether the agent is a server or not

	virtual int command(int argc, const char*const* argv);
	virtual void recv(Packet*, Handler*);//TODO: Almost done

	void SetDataSize(int s){ datasize_ = s;	size_ = s;}                //TODO: Done
	static const vector<int> GetPktSizeList(){ return pktsize_list_; } //TODO: Done
	bool GetNextTarget_HDR();        //TODO: Almost done
	bool GetNextTarget_MW(int root); //TODO: Almost done
	bool GetNextTarget_PF();         //TODO: Almost done
	bool GetNextTarget_NOVA();       //TODO: Almost done
	void UpdateRate(int index);      //TODO: Almost done
	void Reset_basic();              //TODO: Only for server to reset parameters but keep the history
	void Reset_all();                //TODO: Only for server to reset parameters and the history

protected:
	u_int32_t Ackaddr_;    // IP address for incoming ACK packet
	double slot_interval_; // timelength of a single slot
	int numslot_;          // number of slots in a simulation
	int datasize_;         // size of payload
	unsigned int current_slot_;     // index of the current slot
	Mac* mac_;             // MAC
	VoD_ScheduleClient* target_;    // Only for server: showing the current target client 
	ofstream tracefile_;   // For outputting user-defined trace file
	bool hold_;			   // Indicate whether there are multiple trials to be run and averaged
	int numtrial_;         // Number of trials to be run at a time
	int mwroot_;		   // Specify the order of Max-Weight policy
	double epsilon_NOVA_;  // For NOVA policy
	double beta_NOVA_;	   // For NOVA policy
	double b0_NOVA_;	   // Initial pressure for NOVA policy

	// Indicate number of possible data rates 	
	static const int numrate_;
	// Static data member for specifying data size of a packet
	static const vector<int> pktsize_list_;
};

#endif // ns_vod_schedule_h
