# Copyright (c) 1997 Regents of the University of California.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#      This product includes software developed by the Computer Systems
#      Engineering Group at Lawrence Berkeley Laboratory.
# 4. Neither the name of the University nor of the Laboratory may be used
#    to endorse or promote products derived from this software without
#    specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# simple-wireless.tcl
# A simple example for wireless simulation

# ======================================================================
# Define options
# ======================================================================
set val(chan)           Channel/WirelessChannel    ;# channel type
set val(prop)           Propagation/Shadowing      ;# radio-propagation model
set val(netif)          Phy/WirelessPhy            ;# network interface type
set val(mac)            Mac/802_11                 ;# MAC type
set val(ifq)            Queue/DropTail/PriQueue    ;# interface queue type
set val(ll)             LL                         ;# link layer type
set val(ant)            Antenna/OmniAntenna        ;# antenna model
set val(ifqlen)         50                         ;# max packet in ifq
set val(nn)             21                         ;# number of mobilenodes
set val(rp)             DumbAgent                  ;# routing protocol


# ======================================================================
# Main Program
# ======================================================================


#
# Initialize Global Variables
#
set ns_		[new Simulator]
set tracefd     [open simVoD.tr w]
$ns_ trace-all $tracefd

# set up topography object
set topo       [new Topography]

$topo load_flatgrid 2010 2010

#
# Create God
#
create-god $val(nn)

#
#  Create the specified number of mobilenodes [$val(nn)] and "attach" them
#  to the channel. 
#  Here two nodes are created : node(0) and node(1)

# configure node
Phy/WirelessPhy set Pt_ 1
Propagation/Shadowing set pathlossExp_ 0.0			 
Mac/802_11 set dataRate_  54.0e6
Mac/802_11 set basicRate_ 54.0e6
Mac/802_11 set CWMin_         1
Mac/802_11 set CWMax_         1
Mac/802_11 set PreambleLength_   144;  			# long preamble 
Mac/802_11 set PLCPDataRate_  6.0e6           ;# 6Mbps
Mac/802_11 set RTSThreshold_  5000
Mac/802_11 set ShortRetryLimit_       1               ;# retransmittions
Mac/802_11 set LongRetryLimit_        1               ;# retransmissions
Mac/802_11 set TxFeedback_ 0;
Mac/802_11 set SlotTime_      0.000009        ;# 9us
Mac/802_11 set SIFS_          0.000016        ;# 16us

#Agent/VoD_Schedule set packetSize_ 160
#Agent/VoD_Schedule set slotInterval_ 0.01
Agent/VoD_Schedule set numSlot_ 1000


set dRNG [new RNG]
$dRNG seed [lindex $argv 0]
$dRNG default


$ns_ node-config -adhocRouting $val(rp) \
				 -llType $val(ll) \
			 	 -macType $val(mac) \
			 	 -ifqType $val(ifq) \
			 	 -ifqLen $val(ifqlen) \
			 	 -antType $val(ant) \
			 	 -propType $val(prop) \
			 	 -phyType $val(netif) \
			 	 -channelType $val(chan) \
			 	 -topoInstance $topo \
			 	 -agentTrace ON\
			 	 -routerTrace OFF \
			 	 -macTrace ON \
			 	 -movementTrace OFF			

set node_(0) [$ns_ node]
$node_(0) set X_ 3
$node_(0) set Y_ 100
$node_(0) set Z_ 0
set vod_(0) [new Agent/VoD_Schedule]
$ns_ attach-agent $node_(0) $vod_(0)


for {set i 1} {$i < $val(nn) } {incr i} {
	set node_($i) [$ns_ node]	
	$node_($i) random-motion 0		;# disable random motion
	$node_($i) set X_ [expr 3.0 + $i*10]
	$node_($i) set Y_ 100
	$node_($i) set Z_ 0
	set vod_($i) [new Agent/VoD_Schedule]
	$ns_ attach-agent $node_($i) $vod_($i)
#	$ns_ connect $vod_($i) $vod_(0)
#	$ns_ at 0.0 "$rt_($i) register"
}

#$ns_ at 0.0 "$vod_(0) server_start"
#$ns_ at 0.1 "$rt_(0) windowT 20"
set mymac [$node_(0) set mac_(0)]
$ns_ at 0.0 "$vod_(0) mac $mymac"
$ns_ at 0.5 "$vod_(0) server"
#$mymac setTxFeedback 1

set slottime 0.0005			;#application layer slot time = 500us

for {set j 1} {$j < 6} {incr j} {
	$ns_ connect $vod_($j) $vod_(0)
	$ns_ at [expr ($j) + 2] "$vod_($j) register 78 1 0 4 66"
}

for {set j 6} {$j < 11} {incr j} {
	$ns_ connect $vod_($j) $vod_(0)
	$ns_ at [expr ($j) + 2] "$vod_($j) register 156 1 0 1 66"
}

for {set j 11} {$j < 16} {incr j} {
	$ns_ connect $vod_($j) $vod_(0)
	$ns_ at [expr ($j) + 2] "$vod_($j) register 78 1 0 4 66"
}

for {set j 16} {$j < 21} {incr j} {
	$ns_ connect $vod_($j) $vod_(0)
	$ns_ at [expr ($j) + 2] "$vod_($j) register 156 1 0 1 66"
}
#$ns_ at 9.0  "$vod_(0) config-MW 10"
$ns_ at 50.0 "$vod_(0) config-NOVA 0.05 0 0"


set period 100.0

#for {set k 0} {$k < 5} {incr k} {
#	for {set i 0} {$i < 30000} {incr i} {
#		$ns_ at [expr $period*($k + 1) + $i/1000.0] "$vod_(0) send HDR"
#		puts "Iteration $k-$i"
#	}
#	$ns_ at [expr $period*($k + 2) - 0.002] "$vod_(0) report-short ./user-tracefile/report-HDR-[expr $k + 46].txt  5" 
#	$ns_ at [expr $period*($k + 2) - 0.001] "$vod_(0) restart-reset" 
#}

#for {set k 0} {$k < 5} {incr k} {
#	$ns_ at [expr 100.0*($k + 1) - 0.002] "$vod_(0) config-MW 1" 
#	for {set i 0} {$i < 30000} {incr i} {
#		$ns_ at [expr 100.0*($k + 1) + $i/1000.0] "$vod_(0) send MW"
#		puts "Iteration $k-$i"
#	}
#	$ns_ at [expr 100.0*($k + 2) - 0.002] "$vod_(0) report-short ./user-tracefile/report-MW-[expr $k + 31].txt  5" 
#	$ns_ at [expr 100.0*($k + 2) - 0.001] "$vod_(0) restart-reset" 
#}

########################## For r-order MW ##################################

#for {set k 0} {$k < 5} {incr k} {
#	$ns_ at [expr 100.0*($k + 1) - 0.002] "$vod_(0) config-MW 10" 
#	for {set i 0} {$i < 30000} {incr i} {
#		$ns_ at [expr 100.0*($k + 1) + $i/1000.0] "$vod_(0) send MW"
#		puts "Iteration $k-$i"
#	}
#	$ns_ at [expr 100.0*($k + 2) - 0.003] "$vod_(0) report-short ./user-tracefile/report-MW-r10-[expr $k + 46].txt  5" 
#	$ns_ at [expr 100.0*($k + 2) - 0.002] "$vod_(0) restart-reset" 
#}

############################################################################

#for {set k 0} {$k < 5} {incr k} {
#	for {set i 0} {$i < 30000} {incr i} {
#		$ns_ at [expr $period*($k + 1) + $i/1000.0] "$vod_(0) send PF"
#		puts "Iteration $k-$i"
#	}
#	$ns_ at [expr $period*($k + 2) - 0.002] "$vod_(0) report-short ./user-tracefile/report-PF-[expr $k + 46].txt  5" 
#	$ns_ at [expr $period*($k + 2) - 0.001] "$vod_(0) restart-reset" 
#}

#for {set k 0} {$k < 5} {incr k} {
#	for {set i 0} {$i < 30000} {incr i} {
#		$ns_ at [expr $period*($k + 1) + $i/1000.0] "$vod_(0) send NOVA"
#		puts "Iteration $k-$i"
#	}
#	$ns_ at [expr $period*($k + 2) - 0.002] "$vod_(0) report-short ./user-tracefile/report-NOVA-b-[expr $k + 46].txt  5" 
#	$ns_ at [expr $period*($k + 2) - 0.001] "$vod_(0) restart-reset" 
#}

################################ Multiple Trials ##############################################

for {set k 0} {$k < 5} {incr k} {
	if [expr $k > 0] {
		$ns_ at [expr $period*($k + 101) - $slottime] "$vod_(0) restart-hold" 		
	}
	for {set i 0} {$i < 50145} {incr i} {
		$ns_ at [expr $period*($k + 101) + $i*$slottime] "$vod_(0) send HDR"
#		puts "Iteration $k-$i"
	}
}

#Schedule events
$ns_ at 20000.0 "$vod_(0) report-short ./user-tracefile/report-Q-HDR-1(x5).txt  5"
#$ns_ at 20000.0 "$vod_(0) report-all ./user-tracefile/report-B-HDR-1(x1).txt  5"
$ns_ at 20001.0 "$vod_(0) restart-reset"


#for {set k 0} {$k < 5} {incr k} {
#		$ns_ at [expr $period*($k + 301) - 2*$slottime] "$vod_(0) config-MW 1" 
#	if [expr $k > 0] {
#		$ns_ at [expr $period*($k + 301) - $slottime] "$vod_(0) restart-hold" 		
#	}
#	for {set i 0} {$i < 50000} {incr i} {
#		$ns_ at [expr $period*($k + 301) + $i*$slottime] "$vod_(0) send MW"
#		puts "Iteration $k-$i"
#	}
#}

#$ns_ at 40000.0 "$vod_(0) report-short ./user-tracefile/report-B-MW-1(x5).txt  5" 
#$ns_ at 40001.0 "$vod_(0) restart-reset"

########################## For r-order MW ##################################
#for {set k 0} {$k < 5} {incr k} {
#	$ns_ at [expr $period*($k + 501) - 2*$slottime] "$vod_(0) config-MW 10" 
#	if [expr $k > 0] {
#		$ns_ at [expr $period*($k + 501) - $slottime] "$vod_(0) restart-hold" 		
#	}
#	for {set i 0} {$i < 50000} {incr i} {
#		$ns_ at [expr $period*($k + 501) + $i*$slottime] "$vod_(0) send MW"
#		puts "Iteration $k-$i"
#	}
#}

#$ns_ at 60000.0 "$vod_(0) report-short ./user-tracefile/report-B-MW-r10-1(x5).txt  5" 
#$ns_ at 60001.0 "$vod_(0) restart-reset"

############################################################################

#for {set k 0} {$k < 5} {incr k} {
#	if [expr $k > 0] {
#		$ns_ at [expr $period*($k + 701) - $slottime] "$vod_(0) restart-hold" 		
#	}
#	for {set i 0} {$i < 50000} {incr i} {
#		$ns_ at [expr $period*($k + 701) + $i*$slottime] "$vod_(0) send PF"
#		puts "Iteration $k-$i"
#	}
#}

#$ns_ at 80000.0 "$vod_(0) report-short ./user-tracefile/report-B-PF-1(x5).txt  5" 
#$ns_ at 80001.0 "$vod_(0) restart-reset"


#for {set k 0} {$k < 5} {incr k} {
#	if [expr $k > 0] {
#		$ns_ at [expr $period*($k + 901) - $slottime] "$vod_(0) restart-hold" 		
#	}
#	for {set i 0} {$i < 50000} {incr i} {
#		$ns_ at [expr $period*($k + 901) + $i*$slottime] "$vod_(0) send NOVA"
#		puts "Iteration $k-$i"
#	}
#}

#$ns_ at 100000.0 "$vod_(0) report-short ./user-tracefile/report-B-NOVA-1(x5).txt  5" 
#$ns_ at 100001.0 "$vod_(0) restart-reset" 


$ns_ at 110000.0 "stop"


#  for {set i 1} {$i < 30} {incr i} {
#    $ns_ at [expr 1000.0 + $i+0.00001] "$rt_(0) report"
#  }

#$ns_ at 11000.0 "$rt_(0) stop"


Mac/802_11 instproc txfailed {} {
	upvar vod_(0) myvod
	$myvod update_failed 
}

Mac/802_11 instproc txsucceed {} {
	upvar vod_(0) myvod
	$myvod update_delivered 
}

#Mac/802_11 instproc brdsucced {} {
#}

proc stop {} {
    global ns_ tracefd
    #$ns_ flush-trace
    close $tracefd
}

$ns_ run

