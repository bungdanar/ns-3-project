#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/mobility-module.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  /*
  Set first topology/server
  */

  // Should extract node count magic number
  NodeContainer p2pServerNodes;
  p2pServerNodes.Create (2);

  // Should extract magic numbers and set them as project parameters
  PointToPointHelper p2pHelper;
  p2pHelper.SetDeviceAttribute ("DataRate", StringValue ("5mbps"));
  p2pHelper.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Install net device to server nodes
  NetDeviceContainer p2pServerDevices;
  p2pServerDevices = p2pHelper.Install (p2pServerNodes);

  /*
  Set second topology/wired nodes using csma/bus topology
  */

  // Should extract node count magic number and set it as project parameters
  NodeContainer csmaWiredNodes;
  csmaWiredNodes.Add (p2pServerNodes.Get (1));
  csmaWiredNodes.Create (11);

  // Should extract magic numbers and set them as project parameters
  CsmaHelper csmaHelper;
  csmaHelper.SetDeviceAttribute ("DataRate", StringValue ("100mbps"));
  csmaHelper.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  // Install net device to wired nodes
  NetDeviceContainer csmaWiredDevices;
  csmaWiredDevices = csmaHelper.Install (csmaWiredNodes);

  /*
  Set third topology/wireless nodes using wireless topology
  */

  // Should extract node count magic number and set it as project parameters
  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (11);
  NodeContainer wifiApNode = p2pServerNodes.Get (1);

  YansWifiChannelHelper wifiChannelHelper = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper wifiPhysicalHelper;
  wifiPhysicalHelper.SetChannel (wifiChannelHelper.Create ());

  WifiMacHelper wifiMacHelper;
  Ssid wifiSsid = Ssid ("ns-3-ssid");

  WifiHelper wifiHelper;

  // Install net device to wifi station nodes
  NetDeviceContainer wifiStaDevices;
  wifiMacHelper.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (wifiSsid), "ActiveProbing",
                         BooleanValue (false));
  wifiStaDevices = wifiHelper.Install (wifiPhysicalHelper, wifiMacHelper, wifiStaNodes);

  // Install net device to wifi access point node
  NetDeviceContainer wifiApDevices;
  wifiMacHelper.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (wifiSsid));
  wifiApDevices = wifiHelper.Install (wifiPhysicalHelper, wifiMacHelper, wifiApNode);

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

  return 0;
}