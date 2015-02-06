/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/packet-sink.h"
#include "ns3/point-to-point-dumbbell.h"
#include "ns3/random-variable-stream.h"
#include <iostream>
#include <string>

// ECE 6110 - Project 1 - TCP Throughput Measurement
// Hongyang Wang, 903068077
// Jan 14 2015 - Feb 1 2015

// Network Topology
//
//         10ms              20ms              10ms
// n0 -------------- n1 -------------- n2 -------------- n3
//          5mb               1mb               5mb


using namespace ns3;

int main(int argc, char *argv[])
{
    // Parameters needed for this project
    uint32_t nFlows = 1;
    uint32_t queueSize = 64000;
    uint32_t windowSize = 2000;
    uint32_t segSize = 512;
    
    // Enable command line
    CommandLine cmd;
    cmd.AddValue("nFlows", "Number of Flows", nFlows);
    cmd.AddValue("segSize", "Segment Size", segSize);
    cmd.AddValue("queueSize", "Queue Size", queueSize);
    cmd.AddValue("windowSize", "Window Size", windowSize);
    cmd.Parse(argc,argv);
    
    // Set attributes for TCP
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpTahoe"));
    Config::SetDefault("ns3::DropTailQueue::Mode", EnumValue(DropTailQueue::QUEUE_MODE_BYTES));
    Config::SetDefault("ns3::DropTailQueue::MaxBytes", UintegerValue(queueSize));
    Config::SetDefault("ns3::TcpSocketBase::MaxWindowSize", UintegerValue(windowSize));
    Config::SetDefault("ns3::TcpSocketBase::WindowScaling", BooleanValue(false));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(segSize));
    
    // Create a dumbbell topology
    uint32_t nLeftNodes = nFlows;
    uint32_t nRightNodes = nFlows;
    PointToPointHelper p2pLeaf, p2pRouters;
    p2pLeaf.SetDeviceAttribute("DataRate", StringValue ("5Mbps"));
    p2pLeaf.SetChannelAttribute("Delay", StringValue ("10ms"));
    p2pRouters.SetDeviceAttribute("DataRate", StringValue ("1Mbps"));
    p2pRouters.SetChannelAttribute("Delay", StringValue ("20ms"));
    PointToPointDumbbellHelper dumbbell(nLeftNodes, p2pLeaf, nRightNodes, p2pLeaf, p2pRouters);
    
    // Install stack
    InternetStackHelper stack;
    dumbbell.InstallStack(stack);
    
    // Assign IP address
    Ipv4AddressHelper ltIps = Ipv4AddressHelper("10.1.1.0", "255.255.255.0");
    Ipv4AddressHelper rtIps = Ipv4AddressHelper("10.2.1.0", "255.255.255.0");
    Ipv4AddressHelper routerIps = Ipv4AddressHelper("10.3.1.0", "255.255.255.0");
    dumbbell.AssignIpv4Addresses(ltIps, rtIps, routerIps);
    
    // General random start time between 0.0 and 0.1
    RngSeedManager::SetSeed(11223344);
    Ptr<UniformRandomVariable> U = CreateObject<UniformRandomVariable>();
    U -> SetAttribute ("Stream", IntegerValue (6110));
    U -> SetAttribute("Min", DoubleValue(0.0));
    U -> SetAttribute("Max", DoubleValue(0.1));
    double startTime[nFlows];
    for(uint16_t i = 0; i < nFlows; i++) {
        startTime[i] = U -> GetValue();
    }
    
    // Create senders and sinkers
    uint16_t port = 9;
    ApplicationContainer sourceApps, sinkApps;
    for(uint16_t i = 0; i < nFlows; i++) {
        BulkSendHelper source("ns3::TcpSocketFactory",
                              InetSocketAddress(dumbbell.GetRightIpv4Address(i), port + i));
        source.SetAttribute("SendSize", UintegerValue(512));
        source.SetAttribute("MaxBytes", UintegerValue(0));
        sourceApps.Add(source.Install(dumbbell.GetLeft(i)));
        (sourceApps.Get(i)) -> SetStartTime(Seconds(startTime[i]));
        (sourceApps.Get(i)) -> SetStopTime(Seconds(10.0));
        
        PacketSinkHelper sink("ns3::TcpSocketFactory",
                              InetSocketAddress(Ipv4Address::GetAny(), port + i));
        sinkApps.Add(sink.Install(dumbbell.GetRight(i)));
        (sinkApps.Get(i)) -> SetStartTime(Seconds(0.0));
        (sinkApps.Get(i)) -> SetStopTime(Seconds(10.0));
    }
    
    // Tell how data flows
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    
    // Run the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    
    // Calculate the goodput for every flow
    for(uint16_t i = 0; i < nFlows; i++) {
        Ptr<PacketSink> sink1 = DynamicCast<PacketSink>(sinkApps.Get(i));
        double goodput = sink1 -> GetTotalRx() / (10.0 - startTime[i]);
        std::cout << "flow " << i << " windowSize " << windowSize
        << " queueSize " << queueSize << " segSize " << segSize
        << " goodput " << goodput << std::endl;
    }
    
    return 0;
}
