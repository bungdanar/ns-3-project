#include "iostream"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"

using namespace std;
using namespace ns3;

int
main (int argc, char *argv[])
{
  auto globalDataRate = StringValue ("100Mbps");
  auto globalDelay = StringValue ("1ms");

  int nRouter = 3;
  int nServer = 1;
  int nWiredclient = 4;
  int nWifiSta = 4;
  int nBot = 4;

  int idxRouterForServer = 0;
  int idxRouterForWired = 1;
  int idxRouterForWireless = 2;

  // Routers
  NodeContainer routerNodes;
  routerNodes.Create (nRouter);

  PointToPointHelper ppR01, ppR02;
  ppR01.SetDeviceAttribute ("DataRate", globalDataRate);
  ppR01.SetChannelAttribute ("Delay", globalDelay);

  ppR02.SetDeviceAttribute ("DataRate", globalDataRate);
  ppR02.SetChannelAttribute ("Delay", globalDelay);

  NetDeviceContainer dR01, dR02;
  dR01 = ppR01.Install (routerNodes.Get (idxRouterForServer), routerNodes.Get (idxRouterForWired));
  dR02 =
      ppR02.Install (routerNodes.Get (idxRouterForServer), routerNodes.Get (idxRouterForWireless));

  // Server
  NodeContainer serverNode;
  serverNode.Create (nServer);

  PointToPointHelper ppS0R0;
  ppS0R0.SetDeviceAttribute ("DataRate", globalDataRate);
  ppS0R0.SetChannelAttribute ("Delay", globalDelay);

  NetDeviceContainer dS0R0;
  dS0R0 = ppS0R0.Install (serverNode.Get (0), routerNodes.Get (idxRouterForServer));

  // Wired clients
  NodeContainer wiredClientNodes;
  wiredClientNodes.Add (routerNodes.Get (idxRouterForWired));
  wiredClientNodes.Create (nWiredclient);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", globalDataRate);
  csma.SetChannelAttribute ("Delay", globalDelay);

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
  ppBot.SetDeviceAttribute ("DataRate", globalDataRate);
  ppBot.SetChannelAttribute ("Delay", globalDelay);

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

  // Populate routing table
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

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
  mobilityHelper.SetMobilityModel ("ns3::RandomWalk2dMobilityModel", "Bounds",
                                   RectangleValue (Rectangle (-50, 50, -50, 50)));
  mobilityHelper.Install (wifiStaNodes);

  // Just some logging
  // Delete this afterward
  cout << "n Router: " << routerNodes.GetN () << endl
       << "n Server: " << serverNode.GetN () << endl
       << "n Wired: " << wiredClientNodes.GetN () << endl
       << "n Wireless Ap: " << wifiApNode.GetN () << endl
       << "n Wireless Sta: " << wifiStaNodes.GetN () << endl
       << "n Bot: " << botNodes.GetN () << endl;

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

  // Run the Simulation
  Simulator::Stop (Seconds (10));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}