#include "iostream"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

// using namespace std;
using namespace ns3;

std::string dir;
uint32_t prev = 0;
Time prevTime = Seconds (0);

// RTT stuff
static std::map<uint32_t, bool> firstRtt;
static std::map<uint32_t, Ptr<OutputStreamWrapper>> rttStream;

// Packet loss stuff
static std::map<uint32_t, Ptr<OutputStreamWrapper>> nextTxStream;
static std::map<uint32_t, Ptr<OutputStreamWrapper>> nextRxStream;

// Naming the output directory using local system time
static void
handleOutputDirName ()
{
  time_t rawtime;
  struct tm *timeinfo;
  char buffer[80];
  time (&rawtime);
  timeinfo = localtime (&rawtime);
  strftime (buffer, sizeof (buffer), "%d-%m-%Y-%I-%M-%S", timeinfo);
  std::string currentTime (buffer);
  dir = "congestion-ddos-results/" + currentTime + "/";
  std::string dirToSave = "mkdir -p " + dir;
  if (system (dirToSave.c_str ()) == -1)
    {
      exit (1);
    }
}

// RTT stuff
static uint32_t
GetNodeIdFromContext (std::string context)
{
  std::size_t const n1 = context.find_first_of ("/", 1);
  std::size_t const n2 = context.find_first_of ("/", n1 + 1);
  return std::stoul (context.substr (n1 + 1, n2 - n1 - 1));
}

// RTT stuff
static void
RttTracer (std::string context, Time oldval, Time newval)
{
  uint32_t nodeId = GetNodeIdFromContext (context);

  if (firstRtt[nodeId])
    {
      *rttStream[nodeId]->GetStream () << "0.0 " << oldval.GetSeconds () << std::endl;
      firstRtt[nodeId] = false;
    }
  *rttStream[nodeId]->GetStream ()
      << Simulator::Now ().GetSeconds () << " " << newval.GetSeconds () << std::endl;
}

// RTT stuff
static void
TraceRtt (std::string rtt_tr_file_name, uint32_t nodeId)
{
  AsciiTraceHelper ascii;
  rttStream[nodeId] = ascii.CreateFileStream (rtt_tr_file_name.c_str ());
  Config::Connect ("/NodeList/" + std::to_string (nodeId) + "/$ns3::TcpL4Protocol/SocketList/0/RTT",
                   MakeCallback (&RttTracer));
}

// Packet loss stuff
static void
NextTxTracer (std::string context, [[maybe_unused]] SequenceNumber32 old, SequenceNumber32 nextTx)
{
  uint32_t nodeId = GetNodeIdFromContext (context);

  *nextTxStream[nodeId]->GetStream ()
      << Simulator::Now ().GetSeconds () << " " << nextTx << std::endl;
}

static void
NextRxTracer (std::string context, [[maybe_unused]] SequenceNumber32 old, SequenceNumber32 nextRx)
{
  uint32_t nodeId = GetNodeIdFromContext (context);

  *nextRxStream[nodeId]->GetStream ()
      << Simulator::Now ().GetSeconds () << " " << nextRx << std::endl;
}

static void
TraceNextTx (std::string &next_tx_seq_file_name, uint32_t nodeId)
{
  AsciiTraceHelper ascii;
  nextTxStream[nodeId] = ascii.CreateFileStream (next_tx_seq_file_name.c_str ());
  Config::Connect ("/NodeList/" + std::to_string (nodeId) +
                       "/$ns3::TcpL4Protocol/SocketList/0/NextTxSequence",
                   MakeCallback (&NextTxTracer));
}

static void
TraceNextRx (std::string &next_rx_seq_file_name, uint32_t nodeId)
{
  AsciiTraceHelper ascii;
  nextRxStream[nodeId] = ascii.CreateFileStream (next_rx_seq_file_name.c_str ());
  Config::Connect ("/NodeList/" + std::to_string (nodeId) +
                       "/$ns3::TcpL4Protocol/SocketList/1/RxBuffer/NextRxSequence",
                   MakeCallback (&NextRxTracer));
}

// Calculate throughput
static void
TraceThroughput (Ptr<FlowMonitor> monitor)
{
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  auto itr = stats.begin ();
  Time curTime = Now ();
  std::ofstream thr (dir + "/throughput.dat", std::ios::out | std::ios::app);
  thr << curTime << " "
      << 8 * (itr->second.txBytes - prev) /
             (1000 * 1000 * (curTime.GetSeconds () - prevTime.GetSeconds ()))
      << std::endl;
  prevTime = curTime;
  prev = itr->second.txBytes;
  Simulator::Schedule (Seconds (0.2), &TraceThroughput, monitor);
}

int
main (int argc, char *argv[])
{
  auto dataRate_p2pR01 = StringValue ("10Mbps");
  auto delay_p2pR01 = StringValue ("1ms");

  auto dataRate_p2pR02 = StringValue ("10Mbps");
  auto delay_p2pR02 = StringValue ("1ms");

  auto dataRate_p2pS0R0 = StringValue ("10Mbps");
  auto delay_p2pS0R0 = StringValue ("1ms");

  auto dataRate_csma = StringValue ("10Mbps");
  auto delay_csma = StringValue ("1ms");

  auto dataRate_p2pBot = StringValue ("10Mbps");
  auto delay_p2pBot = StringValue ("1ms");

  std::string dataRate_ddos = "20480kb/s";

  uint32_t nRouter = 3;
  uint32_t nServer = 1;
  uint32_t nWiredclient = 4;
  uint32_t nWifiSta = 4;
  uint32_t nBot = 4;
  bool enableDdos = true;
  std::string attackType = "udp";

  int idxRouterForServer = 0;
  int idxRouterForWired = 1;
  int idxRouterForWireless = 2;

  int tcpSinkPort = 9000;
  int udpSinkPort = 9001;
  // int maxBulkBytes = 100'000;
  int maxSimulationTime = 10;

  double throughputTraceTime = 0 + 0.001;

  CommandLine cmd (__FILE__);
  cmd.AddValue ("n_wired_client", "Jumlah wired node client", nWiredclient);
  cmd.AddValue ("n_wireless_client", "Jumlah wireless node client", nWifiSta);
  cmd.AddValue ("n_bot", "Jumlah bot node client", nBot);
  cmd.AddValue ("enable_ddos", "Enable or disable DDoS attack", enableDdos);
  cmd.AddValue ("attack_type", "DDoS attack type: udp, tcp, http", attackType);
  cmd.Parse (argc, argv);
  if (nWiredclient < 2)
    {
      nWiredclient = 2;
    }

  // TCP congestion control configuration
  std::string tcpTypeId = "TcpBbr";
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::" + tcpTypeId));

  // Routers
  NodeContainer routerNodes;
  routerNodes.Create (nRouter);

  PointToPointHelper ppR01, ppR02;
  ppR01.SetDeviceAttribute ("DataRate", dataRate_p2pR01);
  ppR01.SetChannelAttribute ("Delay", delay_p2pR01);

  ppR02.SetDeviceAttribute ("DataRate", dataRate_p2pR02);
  ppR02.SetChannelAttribute ("Delay", delay_p2pR02);

  NetDeviceContainer dR01, dR02;
  dR01 = ppR01.Install (routerNodes.Get (idxRouterForServer), routerNodes.Get (idxRouterForWired));
  dR02 =
      ppR02.Install (routerNodes.Get (idxRouterForServer), routerNodes.Get (idxRouterForWireless));

  // Server
  NodeContainer serverNode;
  serverNode.Create (nServer);

  PointToPointHelper ppS0R0;
  ppS0R0.SetDeviceAttribute ("DataRate", dataRate_p2pS0R0);
  ppS0R0.SetChannelAttribute ("Delay", delay_p2pS0R0);

  NetDeviceContainer dS0R0;
  dS0R0 = ppS0R0.Install (serverNode.Get (0), routerNodes.Get (idxRouterForServer));

  // Wired clients
  NodeContainer wiredClientNodes;
  wiredClientNodes.Add (routerNodes.Get (idxRouterForWired));
  wiredClientNodes.Create (nWiredclient);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", dataRate_csma);
  csma.SetChannelAttribute ("Delay", delay_csma);

  NetDeviceContainer csmaDevices;
  csmaDevices = csma.Install (wiredClientNodes);

  // Wireless clients
  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (nWifiSta);
  NodeContainer wifiApNode = routerNodes.Get (idxRouterForWireless);

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper wifiPhysical;
  wifiPhysical.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  Ssid wifiSsid = Ssid ("ns-3-ssid");

  WifiHelper wifi;

  // Install net device to wifi station nodes
  NetDeviceContainer wifiStaDevices;
  wifiMac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (wifiSsid), "ActiveProbing",
                   BooleanValue (false));
  wifiStaDevices = wifi.Install (wifiPhysical, wifiMac, wifiStaNodes);

  // Install net device to wifi access point node
  NetDeviceContainer wifiApDevices;
  wifiMac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (wifiSsid));
  wifiApDevices = wifi.Install (wifiPhysical, wifiMac, wifiApNode);

  /*
    Set bots topology using simple point-to-point (for now)
  */
  NodeContainer botNodes;
  botNodes.Create (nBot);

  PointToPointHelper ppBot;
  ppBot.SetDeviceAttribute ("DataRate", dataRate_p2pBot);
  ppBot.SetChannelAttribute ("Delay", delay_p2pBot);

  // Install net device to bot nodes
  NetDeviceContainer botDevices[nBot];
  for (size_t i = 0; i < nBot; i++)
    {
      botDevices[i] = ppBot.Install (routerNodes.Get (idxRouterForServer), botNodes.Get (i));
    }

  /*
    Install Internet Stack to all nodes
  */
  InternetStackHelper stack;
  stack.Install (routerNodes);
  stack.Install (serverNode.Get (0));

  for (size_t i = 1; i < wiredClientNodes.GetN (); i++)
    {
      stack.Install (wiredClientNodes.Get (i));
    }

  // No need because already installed in router section
  // stack.Install (wifiApNode);

  stack.Install (wifiStaNodes);
  stack.Install (botNodes);

  /*
    Set IPv4 address
  */
  Ipv4AddressHelper address;

  // For server network
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer serverInterfaces;
  serverInterfaces = address.Assign (dS0R0);

  // For R01
  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer r01Interfaces;
  r01Interfaces = address.Assign (dR01);

  // For R02
  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer r02Interfaces;
  r02Interfaces = address.Assign (dR02);

  // For wired network
  address.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer wiredInterfaces;
  wiredInterfaces = address.Assign (csmaDevices);

  // For wireless network
  address.SetBase ("10.1.5.0", "255.255.255.0");
  address.Assign (wifiStaDevices);
  address.Assign (wifiApDevices);

  // For bots network
  address.SetBase ("10.1.6.0", "255.255.255.252");
  for (size_t i = 0; i < nBot; i++)
    {
      address.Assign (botDevices[i]);
      address.NewNetwork ();
    }

  // Set node mobility
  MobilityHelper mobilityHelper;

  mobilityHelper.SetPositionAllocator ("ns3::GridPositionAllocator", "MinX", DoubleValue (0.0),
                                       "MinY", DoubleValue (0.0), "DeltaX", DoubleValue (5.0),
                                       "DeltaY", DoubleValue (10.0), "GridWidth", UintegerValue (5),
                                       "LayoutType", StringValue ("RowFirst"));

  // Set constant for router, server, wired, wifiAp,and bot
  mobilityHelper.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobilityHelper.Install (routerNodes);
  mobilityHelper.Install (serverNode);
  mobilityHelper.Install (wiredClientNodes);
  mobilityHelper.Install (wifiApNode);
  mobilityHelper.Install (botNodes);

  // Set random walk for wifiSta
  double rectSize = 50 * nBot;

  mobilityHelper.SetMobilityModel (
      "ns3::RandomWalk2dMobilityModel", "Bounds",
      RectangleValue (Rectangle (-rectSize, rectSize, -rectSize, rectSize)));
  mobilityHelper.Install (wifiStaNodes);

  // Just some logging
  // Delete this afterward
  // std::cout << "n Router: " << routerNodes.GetN () << std::endl
  //           << "n Server: " << serverNode.GetN () << std::endl
  //           << "n Wired: " << wiredClientNodes.GetN () << std::endl
  //           << "n Wireless Ap: " << wifiApNode.GetN () << std::endl
  //           << "n Wireless Sta: " << wifiStaNodes.GetN () << std::endl
  //           << "n Bot: " << botNodes.GetN () << std::endl;

  // UDP sink on server side
  PacketSinkHelper udpSink ("ns3::UdpSocketFactory",
                            Address (InetSocketAddress (Ipv4Address::GetAny (), udpSinkPort)));
  ApplicationContainer udpSinkApp = udpSink.Install (serverNode.Get (0));
  udpSinkApp.Start (Seconds (0.0));
  udpSinkApp.Stop (Seconds (maxSimulationTime));

  // TCP sink on server side
  PacketSinkHelper tcpSink ("ns3::TcpSocketFactory",
                            InetSocketAddress (Ipv4Address::GetAny (), tcpSinkPort));
  ApplicationContainer tcpSinkApp = tcpSink.Install (serverNode.Get (0));
  tcpSinkApp.Start (Seconds (0.0));
  tcpSinkApp.Stop (Seconds (maxSimulationTime));

  // DDoS UDP/TCP/HTTP Flood aplication behaviour
  if (enableDdos)
    {
      if (attackType == "tcp")
        {
          std::cout << "Simulate with DDoS TCP flood attack..." << std::endl;

          OnOffHelper onoff (
              "ns3::TcpSocketFactory",
              Address (InetSocketAddress (serverInterfaces.GetAddress (0), tcpSinkPort)));
          onoff.SetConstantRate (DataRate (dataRate_ddos));
          onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=30]"));
          onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

          ApplicationContainer onOffUdpBotApps[nBot];
          for (uint32_t k = 0; k < nBot; ++k)
            {
              onOffUdpBotApps[k] = onoff.Install (botNodes.Get (k));
              onOffUdpBotApps[k].Start (Seconds (0.0));
              onOffUdpBotApps[k].Stop (Seconds (maxSimulationTime));
            }
        }
      else
        {
          std::cout << "Simulate with DDoS UDP flood attack..." << std::endl;

          OnOffHelper onoff (
              "ns3::UdpSocketFactory",
              Address (InetSocketAddress (serverInterfaces.GetAddress (0), udpSinkPort)));
          onoff.SetConstantRate (DataRate (dataRate_ddos));
          onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=30]"));
          onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

          ApplicationContainer onOffUdpBotApps[nBot];
          for (uint32_t k = 0; k < nBot; ++k)
            {
              onOffUdpBotApps[k] = onoff.Install (botNodes.Get (k));
              onOffUdpBotApps[k].Start (Seconds (0.0));
              onOffUdpBotApps[k].Stop (Seconds (maxSimulationTime));
            }
        }
    }
  else
    {
      std::cout << "Simulate without DDoS attack..." << std::endl;
    }

  // Build legitimate TCP sender application for wired and wireless
  BulkSendHelper bulkSend ("ns3::TcpSocketFactory",
                           InetSocketAddress (serverInterfaces.GetAddress (0), tcpSinkPort));
  bulkSend.SetAttribute ("MaxBytes", UintegerValue (0));

  ApplicationContainer wiredBulkSendApps[nWiredclient];
  for (size_t i = 1; i < wiredClientNodes.GetN (); i++)
    {
      wiredBulkSendApps[i] = bulkSend.Install (wiredClientNodes.Get (i));
      wiredBulkSendApps[i].Start (Seconds (0.0));
      wiredBulkSendApps[i].Stop (Seconds (maxSimulationTime));
    }

  ApplicationContainer wifiBulkSendApps[nWifiSta];
  for (size_t i = 0; i < wifiStaNodes.GetN (); i++)
    {
      wifiBulkSendApps[i] = bulkSend.Install (wifiStaNodes.Get (i));
      wifiBulkSendApps[i].Start (Seconds (0.0));
      wifiBulkSendApps[i].Stop (Seconds (maxSimulationTime));
    }

  // Populate routing table
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // NetAnim
  AnimationInterface anim ("DDoSim.xml");
  const int yServer = 0;
  const int yWired = 10;
  const int yWifiAp = 20;
  const int yBot = -20;

  // Server position
  AnimationInterface::SetConstantPosition (serverNode.Get (0), 0, yServer);
  AnimationInterface::SetConstantPosition (routerNodes.Get (idxRouterForServer), 10, yServer);

  // Wired position
  AnimationInterface::SetConstantPosition (routerNodes.Get (idxRouterForWired), 20, yWired);
  int currXwired = 30;
  for (size_t i = 1; i < wiredClientNodes.GetN (); i++)
    {
      AnimationInterface::SetConstantPosition (wiredClientNodes.Get (i), currXwired, yWired);
      currXwired += 10;
    }

  // Wireless position
  AnimationInterface::SetConstantPosition (routerNodes.Get (idxRouterForWireless), 20, yWifiAp);
  // int currXWifi = 30;
  // for (size_t i = 0; i < wifiStaNodes.GetN (); i++)
  //   {
  //     AnimationInterface::SetConstantPosition (wifiStaNodes.Get (i), currXWifi, yWifiAp);
  //     currXWifi += 5;
  //   }

  // Bot position
  int currXbot = 0;
  for (size_t i = 0; i < botNodes.GetN (); i++)
    {
      AnimationInterface::SetConstantPosition (botNodes.Get (i), currXbot, yBot);
      currXbot += 5;
    }

  // Create a new directory to store the output of the program
  handleOutputDirName ();

  // RTT and packet loss stuff
  uint16_t num_flows = 1;
  double start_time = 0.1;
  for (uint16_t index = 0; index < num_flows; index++)
    {
      std::string flowString ("");
      if (num_flows > 1)
        {
          flowString = "-flow" + std::to_string (index);
        }

      auto targetNodeId = wiredClientNodes.Get (1)->GetId ();
      auto serverNodeId = serverNode.Get (0)->GetId ();
      firstRtt[targetNodeId] = true;

      Simulator::Schedule (Seconds (start_time * index + throughputTraceTime), &TraceRtt,
                           dir + "/rtt.dat", targetNodeId);

      Simulator::Schedule (Seconds (start_time * index + throughputTraceTime), &TraceNextTx,
                           dir + "/tx.dat", targetNodeId);

      Simulator::Schedule (Seconds (start_time * index + 0.1), &TraceNextRx, dir + "/rx.dat",
                           serverNodeId);
    }

  // Flow Monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.Install (wiredClientNodes.Get (1));
  Simulator::Schedule (Seconds (throughputTraceTime), &TraceThroughput, monitor);

  // Test pcap on server side
  ppS0R0.EnablePcapAll ("server");
  wifiPhysical.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
  wifiPhysical.EnablePcap ("wifiRouter", wifiApDevices.Get (0));

  // Run the Simulation
  Simulator::Stop (Seconds (maxSimulationTime));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}