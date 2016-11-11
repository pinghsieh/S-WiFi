/*
 * vod_schedule.cc
 * Author: Ping-Chun Hsieh (2015)
 */
// File: Code for a new scheduling policy for Video-on-Demand applications
//

#include "vod_schedule.h"
#include <cstdlib>
#include <cmath>      // For pow(a,b)
#include <algorithm>  // For max(a,b)
#include "random.h"   // For Random::random()
using std::max;
using std::pow;

//int list[] = {2340, 2080, 1560, 750, 220, 0};
int list[] = {2340, 2080, 1560, 750, 220};
//int list[] = {1440, 1280, 960, 640, 480, 0};
//int list[] = {720, 360, 0};
vector<int> size_list(list, list + sizeof(list)/sizeof(int)); 
const vector<int> VoD_ScheduleAgent::pktsize_list_ = size_list;
const int VoD_ScheduleAgent::numrate_ = 5;
//const int VoD_ScheduleAgent::numrate_ = 3;


int hdr_vod_schedule::offset_;
static class VoD_ScheduleHeaderClass : public PacketHeaderClass {
public:
	VoD_ScheduleHeaderClass() : PacketHeaderClass("PacketHeader/VoD_Schedule", 
					      sizeof(hdr_vod_schedule)) {
		bind_offset(&hdr_vod_schedule::offset_);
	}
} class_vod_schedulehdr;


static class VoD_ScheduleClass : public TclClass {
public:
	VoD_ScheduleClass() : TclClass("Agent/VoD_Schedule") {}
	TclObject* create(int, const char*const*) {
		return (new VoD_ScheduleAgent());
	}
} class_vod_schedule;


VoD_ScheduleClient::VoD_ScheduleClient() : addr_(0), done_(false), Xn_(0) 
{
	qn_ = 0;	    //throughput requirement
	tier_ = 1;	    //tier of this client
	pn_ = 0;        //channel reliability
	wn_ = 1;        //weight factor
	outage_history_ = vector<int>();	// record the number of outage throughout the simulation trial
	buffer_history_ = vector<int>();	// record the buffer length at the beginning of each time slot
	past_delivery_ = vector<int>();     // record the delivered data in bits in each time slot
	channel_dist_ = vector<double>();   // Specify the distribution of data rate
	data_stored_ = 0;                   // total bits stored in the playback buffer 
	current_rate_ = 0; 
	outage_ = 0;
	is_active_ = true;
	data_received_ = 0;
	NOVA_index_ = 0;
	frame_ = 1;			 //consume data every frame_ slot
	countdown_ = frame_; //countdown timer for periodic consumption
}

double VoD_ScheduleClient::GetPressure_MW(int root)
{ //TODO: For Weighted Max-Weight Policy
	if (root >= 1) {
		double w = current_rate_*pow(max((-1)*Xn_*wn_, double(0)), 1.0/(double)root);
		return w;
	}
	else {
		return 0;
	}
}

double VoD_ScheduleClient::GetPressure_PF()
{ //TODO: For Weighted Proportional-Fair Policy
	double offset = 0.001; //To avoid zero denominator
	return (double)(qn_*current_rate_/(data_received_ + offset));
}

double VoD_ScheduleClient::GetPressure_NOVA()
{ //TODO: For NOVA Policy
  //1. Choose hNOVA = max(bi,0)
	double pressure = 0;
	double threshold = 20;
	double scaling = 0.1;

	if (NOVA_index_ > threshold) {
		pressure = 0.005*(20*NOVA_index_ + pow((NOVA_index_ - threshold), 2.0));
	}
	else if (NOVA_index_ <= threshold && NOVA_index_ >= 0) {
		pressure = scaling*NOVA_index_;
	}
	else {
		pressure = 0;
	}
	pressure = (pressure*current_rate_*wn_);
	//cout << "pressure = " << pressure << endl;
	//double pressure = current_rate_*max(NOVA_index_, (double)0);
	return pressure;
}

VoD_ScheduleAgent::VoD_ScheduleAgent() : Agent(PT_VoD_Schedule), seq_(0), mac_(0)
{ //TODO: Revise...
	numclient_ = 0;
	client_list_ = vector<VoD_ScheduleClient*>();
	is_server_ = 0;
	current_slot_ = 0;
	datasize_ = 0;
	target_ = 0;
	hold_ = false;
	numtrial_ = 1;
	mwroot_ = 1;
	epsilon_NOVA_ = 0.05;
	beta_NOVA_ = 0;
	b0_NOVA_ = 0;
	slot_interval_ = 0.01;

	bind("packetSize_", &datasize_);
	//bind("slotInterval_", &slot_interval_);
	bind("numSlot_", &numslot_);
}

VoD_ScheduleAgent::~VoD_ScheduleAgent()
{
	for (unsigned int i = 0; i < client_list_.size(); i++) {
		delete client_list_[i];
	}
}

bool VoD_ScheduleAgent::GetNextTarget_HDR()
{ //TODO: 1. Choose a client based on current data rate
  //      2. Decide the packet size 
	double rmax = 0;	
	int id = -1;
	for (int i = 0; i < numclient_; i++) {
		if (id >= 0) {
			if ((client_list_[i]->current_rate_ > rmax) || \
			((client_list_[i]->current_rate_ == rmax) &&  \
			((client_list_[i]->Xn_)*(client_list_[i]->wn_) < (client_list_[id]->Xn_)*(client_list_[id]->wn_)))) {
				id = i;
				rmax = client_list_[i]->current_rate_;
			}
		}
		else {
			if (client_list_[i]->current_rate_ > rmax) {
				id = i;
				rmax = client_list_[i]->current_rate_;
			}
		}
	}
	if (id >= 0) { //TODO
		target_ = client_list_[id]; // Choose target client
		datasize_ = client_list_[id]->current_rate_; // Determine packet sizes
		return true; 
	}
	else { // All clients are not connected
		return false;
	}
}

bool VoD_ScheduleAgent::GetNextTarget_MW(int root)
{ //TODO: 1. Choose a client based on Max-Weight criterion
  //      2. Decide the packet size 
	double wmax = 0;
	int id = -1;
	for (int i = 0; i < numclient_; i++) {
		if (id >= 0) { //TODO: How to break ties? Xn?
			if (((client_list_[i]->GetPressure_MW(root)) > wmax) ||
			((client_list_[i]->GetPressure_MW(root)) == wmax && (client_list_[i]->Xn_ < client_list_[id]->Xn_))) { 
			//((client_list_[i]->GetPressure_MW(root)) == wmax && (client_list_[i]->current_rate_ > client_list_[id]->current_rate_))) {
			//if ((client_list_[i]->GetPressure_MW(root)) > wmax) { //TODO: Check if there is any difference in tie-breaking rules
				id = i;
				wmax = client_list_[i]->GetPressure_MW(root);
			}
		}
		else {
			if (client_list_[i]->current_rate_ > 0) {
				id = i;
				wmax = client_list_[i]->GetPressure_MW(root);					
			}
		}
	}
	if (id >= 0) { //TODO
		target_ = client_list_[id]; // Choose target client
		datasize_ = client_list_[id]->current_rate_; // Determine packet sizes
		return true; 
	}
	else { // All clients are not connected
		return false;
	}
}

bool VoD_ScheduleAgent::GetNextTarget_PF()
{ //TODO: 1. Choose a client based on Proportional-Fair criterion
  //      2. Decide the packet size 
	double fmax = 0;
	int id = -1;
	for (int i = 0; i < numclient_; i++) {
		if (id >= 0) { //TODO: How to break ties? Rn?
			if (((client_list_[i]->GetPressure_PF()) > fmax) ||
			((client_list_[i]->GetPressure_PF()) == fmax && 
            ((client_list_[i]->data_received_/client_list_[i]->qn_) < (client_list_[id]->data_received_/client_list_[id]->qn_)))) {
				id = i;
				fmax = client_list_[i]->GetPressure_PF();
			}
		}
		else {
			if (client_list_[i]->current_rate_ > 0) {
				id = i;
				fmax = client_list_[i]->GetPressure_PF();	
			}
		}
	}
	if (id >= 0) { //TODO
		target_ = client_list_[id]; // Choose target client
		datasize_ = client_list_[id]->current_rate_; // Determine packet sizes
		return true; 
	}
	else { // All clients are not connected
		return false;
	}
}

bool VoD_ScheduleAgent::GetNextTarget_NOVA()
{ //TODO: 1. Choose a client based on NOVA algorithm
  //      2. Decide the packet size 
	double hmax = 0;
	int id = -1;

	for (int i = 0; i < numclient_; i++) {
		if (id >= 0) { //TODO: How to break ties? rn?
			if (((client_list_[i]->GetPressure_NOVA()) > hmax) || \
			//((client_list_[i]->GetPressure_NOVA()) == hmax && (client_list_[i]->NOVA_index_ > client_list_[id]->NOVA_index_))) {
			((client_list_[i]->GetPressure_NOVA()) == hmax && (client_list_[i]->current_rate_ > client_list_[id]->current_rate_))) {
			//if ((client_list_[i]->GetPressure_NOVA()) > hmax) {
				id = i;
				hmax = client_list_[i]->GetPressure_NOVA();
			}
		}
		else {
			if (client_list_[i]->current_rate_ > 0) {
				id = i;
				hmax = client_list_[i]->GetPressure_NOVA();	
			}
		}
	}
	if (id >= 0) { //TODO
		target_ = client_list_[id]; // Choose target client
		datasize_ = client_list_[id]->current_rate_; // Determine packet sizes
		return true; 
	}
	else { // All clients are not connected
		return false;
	}
}

void VoD_ScheduleAgent::UpdateRate(int index)
{
	// Configure unifrom random generator
	//srand(time(NULL));
	//std::default_random_engine generator;
	//std::uniform_int_distribution<int> distribution();
	//randmax = distribution.max();
	double prob = 0;
	if (index >= 0 && index < numclient_) {
		//prob = ((double)(rand()))/(double)RAND_MAX;
		prob = ((double)(Random::random()))/(double)RAND_MAX;
		int j = 0;
		while (j < numrate_) {
			if (prob < (client_list_[index]->channel_dist_[j])) {
				client_list_[index]->current_rate_ = pktsize_list_[j];
				j = numrate_; // Break while loop						
			}
			else {
				j++;
			}
		}
	}
}

void VoD_ScheduleAgent::Reset_basic()
{
	if (is_server_ == true) {
		for (int i = 0; i < numclient_; i++) {
			client_list_[i]->done_ = false;
			client_list_[i]->Xn_ = 0;
			client_list_[i]->is_active_ = true;
			client_list_[i]->outage_ = 0;
			client_list_[i]->data_stored_ = 0;
			client_list_[i]->current_rate_ = 0;
			client_list_[i]->data_received_ = 0;
			client_list_[i]->NOVA_index_ = 0;
			client_list_[i]->countdown_ = client_list_[i]->frame_;
		}
		seq_ = 0;
		target_ = 0;
		datasize_ = 0;
		Ackaddr_ = 0;
	}
}

void VoD_ScheduleAgent::Reset_all()
{//TODO: Reset all parameters and history but keep the network links and channel distribution
	if (is_server_ == true) {
		for (int i = 0; i < numclient_; i++) {
			client_list_[i]->done_ = false;
			client_list_[i]->Xn_ = 0;
			client_list_[i]->NOVA_index_ = 0;
			client_list_[i]->is_active_ = true;
			client_list_[i]->outage_ = 0;
			client_list_[i]->data_stored_ = 0;
			client_list_[i]->current_rate_ = 0;
			client_list_[i]->data_received_ = 0;
			client_list_[i]->countdown_ = client_list_[i]->frame_;
			(client_list_[i]->buffer_history_).clear();
			(client_list_[i]->outage_history_).clear();
			(client_list_[i]->past_delivery_).clear();
			(client_list_[i]->outage_history_).push_back(0);
			(client_list_[i]->buffer_history_).push_back(0);
			(client_list_[i]->past_delivery_).push_back(0);
		}
		seq_ = 0;
		target_ = 0;
		datasize_ = 0;
		hold_ = false;
		numtrial_ = 1;
		Ackaddr_ = 0;
		mwroot_ = 1;
	}
}
// ************************************************
// Table of commands:
// 1. vod_(0) report-all/report-short modulus
// 2. vod_(0) server
// 3. vod_(0) restart-hold
// 4. vod_(0) restart-reset
// 5. vod_(0) send HDR/MW/PF/NOVA
// 6. vod_(0) report-all/report-short name.txt modulus
// 7. vod_(0) mac $mymac
// 8. vod_(0) register $qn $init_buffer $tier $wn
// 9. vod_(0) config-MW $mwroot
//10. vod_(0) config-NOVA $epsilon $beta $bi_0
//
// ************************************************

int VoD_ScheduleAgent::command(int argc, const char*const* argv)
{
	if (argc == 2) {
		if (strcmp(argv[1], "server") == 0) {
			is_server_ = true;
			return (TCL_OK);
		}
		else if (strcmp (argv[1], "update_delivered") == 0){ //TODO: Not applicable...(receive an ACK)
      		int i;
      		for(i = 0; i < numclient_; i++) {
        		if(client_list_[i]->addr_ == Ackaddr_)
          			break;
			}
      		if(i < numclient_){  
				//TODO: If transmission succeeded, set done_ to TRUE
	   			client_list_[i]->done_ = true;
 				//NOTE: Xn_ updated before a new slot at "send"
    		}
      		return (TCL_OK);
    	}
		else if (strcmp(argv[1], "update_failed") == 0){ //TODO: Not applicable...
			printf("failed!\n");
			return (TCL_OK);
    	}
		else if (strcmp (argv[1], "restart-hold") == 0){ //Restart the simulation from time = 0 and keep all the histrory
			hold_ = true;
			current_slot_ = 0;
			numtrial_++;
			Reset_basic();
			return (TCL_OK);
		}
		else if (strcmp (argv[1], "restart-reset") == 0){ //Restart the simulation from time = 0 and clear all the histrory
			current_slot_ = 0;
			Reset_all();
			return (TCL_OK);
		}
	}
	else if (argc == 3) {
		if (strcmp(argv[1], "report-all") == 0) {
		//TODO: Print/Output outage history into a txt file
			tracefile_.open("report.txt");
			int modul = atoi(argv[2]);
			for (int i = 0; i < numclient_; i++) {
				if (i % modul == 0) {
				//if (i % modul == 0 || i % modul == 2) {
					for (unsigned int j = 0; j < (client_list_[i]->buffer_history_).size(); j++) {
						(tracefile_) << "client " << i << ": D(" << j << ") " << ((client_list_[i]->outage_history_)[j]/((double)numtrial_));
						(tracefile_) << "  B(" << j << ") " << ((client_list_[i]->buffer_history_)[j]/((double)numtrial_));
						(tracefile_) << "  R(" << j << ") " << ((client_list_[i]->past_delivery_)[j]/((double)numtrial_)) << endl;
						//cout << "client " << i << ": D(" << j << ")" << (client_list_[i]->outage_history_)[j] << endl;			
					}
				}
				//cout << "Total received data = " << client_list_[i]->data_received_ << endl;
			}
			tracefile_.close();
			return (TCL_OK);
		}
		else if (strcmp(argv[1], "report-short") == 0) {
		//TODO: Print/Output outage history into a txt file
			tracefile_.open("report.txt");
			int modul = atoi(argv[2]);
			for (int i = 0; i < numclient_; i++) {
				if (i % modul == 0) {	
				//if (i % modul == 0 || i % modul == 2) {			
					for (unsigned int j = 0; j < (client_list_[i]->buffer_history_).size(); j++) {
						(tracefile_) << "client " << i << ": D(" << j << ")\t" << ((client_list_[i]->outage_history_)[j]/((double)numtrial_)) << endl;
						//cout << "client " << i << ": D(" << j << ")" << (client_list_[i]->outage_history_)[j] << endl;			
					}
				}
				//cout << "Total received data = " << client_list_[i]->data_received_ << endl;
			}
			tracefile_.close();
			return (TCL_OK);
		}
		else if (strcmp(argv[1], "send") == 0) {
			// Update packet delivery information after the 1st transmission
			if (current_slot_ > 0){
				for (int i = 0; i < numclient_; i++) {
					if (client_list_[i]->done_ == true) {
						client_list_[i]->Xn_ += client_list_[i]->current_rate_;
						client_list_[i]->data_stored_ += client_list_[i]->current_rate_;
						client_list_[i]->data_received_ += client_list_[i]->current_rate_;
						// TODO: Update for NOVA
						client_list_[i]->NOVA_index_ = client_list_[i]->NOVA_index_ - \
						((epsilon_NOVA_*slot_interval_*(client_list_[i]->current_rate_)/(client_list_[i]->qn_)));
						// TODO: If we are repeating a simulation	
						if ((hold_ == true) && (current_slot_ < (client_list_[i]->past_delivery_).size())) {
							(client_list_[i]->past_delivery_)[current_slot_] += client_list_[i]->current_rate_;
						}
						else {
							client_list_[i]->past_delivery_.push_back(client_list_[i]->current_rate_);		
						}	
					}
					else {
						if (!((hold_ == true) && (current_slot_ < (client_list_[i]->past_delivery_).size()))) {
							client_list_[i]->past_delivery_.push_back(0);	
						}			
					}

					if ((hold_ == true) && (current_slot_ < (client_list_[i]->buffer_history_).size())) {
						(client_list_[i]->buffer_history_)[current_slot_] += client_list_[i]->data_stored_;
					}
					else {
						client_list_[i]->buffer_history_.push_back(client_list_[i]->data_stored_);
					}
					// Reset done_ for the new slot
					client_list_[i]->done_ = false;
				}
			}
			current_slot_++;
			// Update countdown timer for periodic consumption
			for (int i = 0; i < numclient_; i++) {
				if (client_list_[i]->countdown_ > 0) {
					client_list_[i]->countdown_ = client_list_[i]->countdown_ - 1;
				}
			}
			// Create a new packet
			Packet* pkt = allocpkt();
			// Access the header for the new packet:
			hdr_vod_schedule* hdr = hdr_vod_schedule::access(pkt);
			// Let the 'ret_' field to 0, so the receiving node
			// knows that it has to generate an echo packet
			hdr->ret_ = 3; // It's a data packet
			hdr->seq_ = seq_++;
			// Store the current time in the 'send_time' field
			hdr->send_time_ = Scheduler::instance().clock();
			// IP information  			
			hdr_ip *ip = hdr_ip::access(pkt);
			// Update channel information in current slot 
			for (int i = 0; i < numclient_; i++) {
				UpdateRate(i);
			}
			// TODO: The following provides options of scheduling policies
			// TODO: Max-Weight Policy
			// TODO: Highest-Data-Rate First Policy
			if (strcmp(argv[2], "HDR") == 0 || strcmp(argv[2], "MW") == 0 || strcmp(argv[2], "PF") == 0 || strcmp(argv[2], "NOVA") == 0) {
				if (strcmp(argv[2], "HDR") == 0) {
					// Choose the target client in this slot
					if (GetNextTarget_HDR() == true) { // "True" means at least one client is ON
						// Set packet size
						size_ = datasize_;
						hdr->datasize_ = datasize_;
						// TODO: If done_ is set to true right after AP sending
						target_->done_ = true;	// TODO: Remeber to modify the 802.11 mac later on!!				

						// Update playback buffer: Do this after receiving an ACK in recv()

						// send data by unicast
						ip->daddr() = target_->addr_;  
						Ackaddr_ = target_->addr_;

						// Send the packet
						send(pkt, 0);
						//cout << "send a packet" << endl;//DEBUG
					}
				}
				else if (strcmp(argv[2], "MW") == 0) {
					// Choose the target client in this slot
					if (GetNextTarget_MW(mwroot_) == true) { // "True" means at least one client is ON
						// Set packet size
						size_ = datasize_;
						hdr->datasize_ = datasize_;
						// TODO: If done_ is set to true right after AP sending
						target_->done_ = true;	// TODO: Remeber to modify the 802.11 mac later on!!	
					
						// Update playback buffer: Do this after receiving an ACK in recv()

						// send data by unicast
						ip->daddr() = target_->addr_;  
						Ackaddr_ = target_->addr_;

						// Send the packet
						send(pkt, 0);
						//cout << "send a packet" << endl;//DEBUG
					}
				}
				else if (strcmp(argv[2], "PF") == 0) {
					// Choose the target client in this slot
					if (GetNextTarget_PF() == true) { // "True" means at least one client is ON
						// Set packet size
						size_ = datasize_;
						hdr->datasize_ = datasize_;
						// TODO: If done_ is set to true right after AP sending
						target_->done_ = true;	// TODO: Remeber to modify the 802.11 mac later on!!	
					
						// Update playback buffer: Do this after receiving an ACK in recv()

						// send data by unicast
						ip->daddr() = target_->addr_;  
						Ackaddr_ = target_->addr_;

						// Send the packet
						send(pkt, 0);
						//cout << "send a packet" << endl;//DEBUG
					}
				}
				else if (strcmp(argv[2], "NOVA") == 0) {
					// Choose the target client in this slot
					if (GetNextTarget_NOVA() == true) { // "True" means at least one client is ON
						// Set packet size
						size_ = datasize_;
						hdr->datasize_ = datasize_;
						// TODO: If done_ is set to true right after AP sending
						target_->done_ = true;	// TODO: Remeber to modify the 802.11 mac later on!!	
					
						// Update playback buffer: Do this after receiving an ACK in recv()

						// send data by unicast
						ip->daddr() = target_->addr_;  
						Ackaddr_ = target_->addr_;

						// Send the packet
						send(pkt, 0);
						//cout << "send a packet" << endl;//DEBUG
					}
				}
				// Update client information
				for (int i=0; i < numclient_; i++) {
				// TODO: Outage occurs with periodic consumption included
					if ((client_list_[i]->countdown_ == 0) && \
						(client_list_[i]->data_stored_ < ((client_list_[i]->qn_)*(client_list_[i]->frame_)))) {
						client_list_[i]->outage_ = (client_list_[i]->outage_) + 1;
						client_list_[i]->Xn_ = client_list_[i]->Xn_ - client_list_[i]->qn_;
						client_list_[i]->NOVA_index_ = client_list_[i]->NOVA_index_ + \
						(epsilon_NOVA_*slot_interval_); // For NOVA
						//(epsilon_NOVA_*slot_interval_/(1.0 + beta_NOVA_)); // For NOVA
					}
				// TODO: Playback continues
					else {
						client_list_[i]->Xn_ = client_list_[i]->Xn_ - client_list_[i]->qn_;	
						client_list_[i]->NOVA_index_ = client_list_[i]->NOVA_index_ + \
						(epsilon_NOVA_*slot_interval_); // For NOVA
						//(epsilon_NOVA_*slot_interval_/(1.0 + beta_NOVA_)); // For NOVA	
						
						// consume data and reset countdown timer
						if (client_list_[i]->countdown_ == 0) {
							client_list_[i]->data_stored_ = client_list_[i]->data_stored_ - (client_list_[i]->qn_)*(client_list_[i]->frame_);
							client_list_[i]->countdown_ = client_list_[i]->frame_;
						}
					}
				// TODO: Maintain outage history
					if ((hold_ == true) && (current_slot_ < (client_list_[i]->outage_history_).size())) {
						(client_list_[i]->outage_history_)[current_slot_] += client_list_[i]->outage_;
					}
					else {
						client_list_[i]->outage_history_.push_back(client_list_[i]->outage_);
					}
					//cout << "NOVA_index_[" << i << "] = " << client_list_[i]->NOVA_index_ << "   " << client_list_[i]->current_rate_<< endl;
				}
            }

			// return TCL_OK, so the calling function knows that
			// the command has been processed
			return (TCL_OK);   
		}

		else if (strcmp(argv[1], "mac") == 0){ 
			mac_ = (Mac*)TclObject::lookup(argv[2]);
			if(mac_ == 0) {
			//TODO: printing...
				printf("mac error!\n");
			}
			return (TCL_OK);
		}

		else if (strcmp(argv[1], "config-MW") == 0) {	
			int root = atoi(argv[2]);
			if (root >= 1) {
				mwroot_ = root;
			}
			else {
				printf("Invalid order for Max-Weight Policy!\n");
			}
			cout << "MW root = " << mwroot_ << endl;
			return (TCL_OK);
		}	
	}
	else if (argc == 4) {
		if (strcmp(argv[1], "report-all") == 0) {
		//TODO: Print/Output outage history into a txt file
			cout << "Start reporting..." << endl;
			cout << "Number of trials = " << numtrial_ << endl;
			tracefile_.open(argv[2]);
			int modul = atoi(argv[3]);
			for (int i = 0; i < numclient_; i++) {
				if (i % modul == 0) {
				//if (i % modul == 0 || i % modul == 2) {
					for (unsigned int j = 0; j < (client_list_[i]->buffer_history_).size(); j++) {
						(tracefile_) << "client " << i << ": D(" << j << ")" << ((client_list_[i]->outage_history_)[j]/((double)numtrial_));
						(tracefile_) << "  B(" << j << ")" << ((client_list_[i]->buffer_history_)[j]/((double)numtrial_));
						(tracefile_) << "  R(" << j << ")" << ((client_list_[i]->past_delivery_)[j]/((double)numtrial_)) << endl;
						//cout << "client " << i << ": D(" << j << ")" << (client_list_[i]->outage_history_)[j] << endl;			
					}
				}
				//cout << "Total received data = " << client_list_[i]->data_received_ << endl;
			}
			tracefile_.close();
			return (TCL_OK);
		}

		else if (strcmp(argv[1], "report-short") == 0) {
		//TODO: Print/Output outage history into a txt file
			cout << "Start reporting..." << endl;
			tracefile_.open(argv[2]);
			int modul = atoi(argv[3]);
			for (int i = 0; i < numclient_; i++) {
				if (i % modul == 0) {
				//if (i % modul == 0 || i % modul == 2) {
					for (unsigned int j = 0; j < (client_list_[i]->buffer_history_).size(); j++) {
						(tracefile_) << "client " << i << ": D(" << j << ")\t" << ((client_list_[i]->outage_history_)[j]/((double)numtrial_)) << endl;
						//cout << "client " << i << ": D(" << j << ")" << (client_list_[i]->outage_history_)[j] << endl;			
					}
				}
				//cout << "Total received data = " << client_list_[i]->data_received_ << endl;
			}
			tracefile_.close();
			return (TCL_OK);
		}
	}

	else if (argc == 5) {
		if (strcmp(argv[1], "config-NOVA") == 0) {	
			double eps = (double)atof(argv[2]);
			double beta = (double)atof(argv[3]);
			double bi0 = (double)atof(argv[4]);

			if (eps >= 0) {	
				epsilon_NOVA_ = eps; 
				//cout << "epsilon = " << epsilon_NOVA_ << endl;
			}
			else { printf("Invalid epsilon for NOVA Policy!\n");}

			if (beta >= 0) { 
				beta_NOVA_ = beta; 
				//cout << "beta = " << beta_NOVA_ << endl;
			}
			else { printf("Invalid beta for NOVA Policy!\n");}

			b0_NOVA_ = bi0;
			//cout << "b0_NOVA = " << b0_NOVA_ << endl;
			for (int i = 0; i < numclient_; i++) {
				client_list_[i]->NOVA_index_ = bi0;
			}
			return (TCL_OK);
		}
	}

	else if (argc == 7) {
		if (strcmp(argv[1], "register") == 0) {
			//TODO: Broadcasting a packet for link registration
			Packet* pkt = allocpkt();
			hdr_vod_schedule* hdr = hdr_vod_schedule::access(pkt);     
			hdr->ret_ = 0;		// a packet to register on the server
			hdr->qn_ = atoi(argv[2]);
			hdr->tier_ = atoi(argv[3]);
			hdr->init_ = atoi(argv[4]);
			hdr->wn_ = atoi(argv[5]);
			hdr->frame_ = atoi(argv[6]);
			send(pkt, 0);     
			return (TCL_OK);
		}
	}  
	// If the command hasn't been processed by VoD_ScheduleAgent()::command,
	// call the command() function for the base class
	return (Agent::command(argc, argv));
}

void VoD_ScheduleAgent::recv(Packet* pkt, Handler*)
{
	// Access the IP header for the received packet:
	hdr_ip* hdrip = hdr_ip::access(pkt);
	//hdr_mac* hdrmac = hdr_mac::access(pkt); // Not in use for now
	
	//cout << "Receive a packet" << endl; //DEBUG

	// Access the VoD scheduling header for the received packet:
	hdr_vod_schedule* hdr = hdr_vod_schedule::access(pkt);

	//this packet is to register  
	if (hdr->ret_ == 0){ 
		if(is_server_ == true){  //only server needs to deal with registration
			//TODO: (Solved) Segmentation error...
			VoD_ScheduleClient* client = new VoD_ScheduleClient;
			client_list_.push_back(client);
			client_list_[numclient_]->addr_ = (u_int32_t)hdrip->saddr();
			client_list_[numclient_]->tier_ = hdr->tier_;
			client_list_[numclient_]->qn_ = hdr->qn_;
			client_list_[numclient_]->data_stored_ = hdr->init_;
			client_list_[numclient_]->frame_ = hdr->frame_;
			client_list_[numclient_]->countdown_ = hdr->frame_;
			client_list_[numclient_]->outage_ = 0;
			client_list_[numclient_]->current_rate_ = 0;
			client_list_[numclient_]->pn_ = 0; // Not in use for now. 
			if (hdr->wn_ > 0) {
				client_list_[numclient_]->wn_ = hdr->wn_;
			}
			else {
				printf("Invalid weight factor!\n");
			}
			//client_list_[numclient_]->done_ = true;
			client_list_[numclient_]->is_active_ = true;
			//TODO: How to better assign the channel distribution?
			if (numclient_ < 10) {
				//double distarr[] = {0.1667, 0.3333, 0.5, 0.6667, 0.8333, 1.0};
				//double distarr[] = {0, 0.2, 0.4, 0.6, 0.8, 1.0};
				//double distarr[] = {0.03333, 0.2, 0.36666, 0.53333, 0.7, 1.0};
				double distarr[] = {0.2, 0.4, 0.6, 0.8, 1.0};
				//double distarr[] = {0.01666, 0.18333, 0.35, 0.51666, 0.68333, 1.0};
				vector<double> dist(distarr, distarr + sizeof(distarr)/sizeof(double));
				client_list_[numclient_]->channel_dist_ = dist; // Evenly-distributed channel distribution
			}
			else {
				//double distarr[] = {0.5, 1.0, 1.0, 1.0, 1.0, 1.0};
				double distarr[] = {0.04, 0.28, 0.52, 0.76, 1.0};
				//double distarr[] = {0.2, 0.4, 0.6, 0.8, 1.0};
				//double distarr[] = {0.1, 0.2, 0.3, 0.4, 1.0};
				vector<double> dist(distarr, distarr + sizeof(distarr)/sizeof(double));
				client_list_[numclient_]->channel_dist_ = dist; // Evenly-distributed channel distribution
			}
			client_list_[numclient_]->outage_history_.push_back(0);
			client_list_[numclient_]->buffer_history_.push_back(0);
			client_list_[numclient_]->past_delivery_.push_back(0);
			numclient_++;
		} 
		Packet::free(pkt);
		return;
	}
    // A data packet from server to client was received. 
	else if (hdr->ret_ == 3){
		// Update playback buffer: data stored and Xn
		//data_stored_ = data_stored_ + hdr->datasize_; //TODO: How to get data size?
		
		// Send an uner-defined ACK to server
/*		Packet* pkt_ACK = allocpkt();
		hdr_vod_schedule* hdr_ACK = hdr_vod_schedule::access(pkt_ACK);
		hdr_ip* hdrip_ACK = hdr_ip::access(pkt_ACK);
		hdrip_ACK->daddr() = (u_int32_t)hdrip->saddr();
		hdr_ACK->ret_ = 7;
		send(pkt_ACK, 0); 
*/
		// Discard the packet
		Packet::free(pkt);
		return;
	}
	// An user-defined ACK packet from client to server
	else if (hdr->ret_ == 7) {
		if(is_server_ == true) {
      		int i;
      		for(i = 0; i < numclient_; i++) {
        		if(client_list_[i]->addr_ == Ackaddr_)
          			break;
			}
    		if(i < numclient_){  
			//TODO: If transmission succeeded, set done_ to TRUE
  				client_list_[i]->done_ = true;
			//NOTE: Xn_ updated before a new slot at "send"
   			}
		}
     	return;
	}
}

