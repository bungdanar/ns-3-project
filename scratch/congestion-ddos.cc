#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  auto globalDataRate = StringValue ("100Mbps");
  auto globalDelay = StringValue ("1ms");

  // Routers
  NodeContainer routerNodes;
  routerNodes.Create (3);

  PointToPointHelper ppR01, ppR02;
  ppR01.SetDeviceAttribute ("DataRate", globalDataRate);
  ppR01.SetChannelAttribute ("Delay", globalDelay);

  ppR02.SetDeviceAttribute ("DataRate", globalDataRate);
  ppR02.SetChannelAttribute ("Delay", globalDelay);

  NetDeviceContainer dR01, dR02;
  dR01 = ppR01.Install (routerNodes.Get (0), routerNodes.Get (1));
  dR02 = ppR02.Install (routerNodes.Get (0), routerNodes.Get (2));

  // Server
  NodeContainer serverNode;
  serverNode.Create (1);

  PointToPointHelper ppS0R0;
  ppS0R0.SetDeviceAttribute ("DataRate", globalDataRate);
  ppS0R0.SetChannelAttribute ("Delay", globalDelay);

  NetDeviceContainer dS0R0;
  dS0R0 = ppS0R0.Install (serverNode.Get (0), routerNodes.Get (0));

  // Wired clients
  NodeContainer wiredClientNodes;
  wiredClientNodes.Add (routerNodes.Get (1));
  wiredClientNodes.Create (10);

  CsmaHelper csma;
  csma.SetDeviceAttribute ("DataRate", globalDataRate);
  csma.SetChannelAttribute ("Delay", globalDelay);

  NetDeviceContainer csmaDevices;
  csmaDevices = csma.Install (wiredClientNodes);

  // Wireless clients
  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (10);
  NodeContainer wifiApNode = routerNodes.Get (2);

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

  // Set wifi mobility
  MobilityHelper mobilityHelper;

  mobilityHelper.SetPositionAllocator ("ns3::GridPositionAllocator", "MinX", DoubleValue (0.0),
                                       "MinY", DoubleValue (0.0), "DeltaX", DoubleValue (5.0),
                                       "DeltaY", DoubleValue (10.0), "GridWidth", UintegerValue (3),
                                       "LayoutType", StringValue ("RowFirst"));

  mobilityHelper.SetMobilityModel ("ns3::RandomWalk2dMobilityModel", "Bounds",
                                   RectangleValue (Rectangle (-50, 50, -50, 50)));
  mobilityHelper.Install (wifiStaNodes);

  mobilityHelper.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobilityHelper.Install (wifiApNode);

  /*
    Set bots topology using simple point-to-point (for now)
  */
  int botNumbers = 50;
  NodeContainer botNodes;
  botNodes.Create (botNumbers);

  PointToPointHelper p2pBot;
  p2pBot.SetDeviceAttribute ("DataRate", globalDataRate);
  p2pBot.SetChannelAttribute ("Delay", globalDelay);

  // Install net device to bot nodes
  NetDeviceContainer botDevices[botNumbers];
  for (size_t i = 0; i < botNumbers; i++)
    {
      botDevices[i] = p2pBot.Install (routerNodes.Get (0), botNodes.Get (i));
    }

  /*
    Install Internet Stack to all nodes
  */
  InternetStackHelper stack;
  stack.Install (routerNodes);
  stack.Install (serverNode);
  stack.Install (wiredClientNodes);
  stack.Install (wifiApNode);
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

  return 0;
}