// -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*-
//
// Copyright (c) 2008 University of Washington
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation;
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#include <vector>
#include <iomanip>
#include "ns3/names.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/net-device.h"
#include "ns3/ipv4-route.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/boolean.h"
#include "ipv4-global-routing.h"
#include "global-route-manager.h"

NS_LOG_COMPONENT_DEFINE ("Ipv4GlobalRouting");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (Ipv4GlobalRouting);

TypeId 
Ipv4GlobalRouting::GetTypeId (void)
{ 
  static TypeId tid = TypeId ("ns3::Ipv4GlobalRouting")
    .SetParent<Object> ()
    .AddAttribute ("RandomEcmpRouting",
                   "Set to true if packets are randomly routed among ECMP; set to false for using only one route consistently",
                   BooleanValue (false),
                   MakeBooleanAccessor (&Ipv4GlobalRouting::m_randomEcmpRouting),
                   MakeBooleanChecker ())
    .AddAttribute ("RespondToInterfaceEvents",
                   "Set to true if you want to dynamically recompute the global routes upon Interface notification events (up/down, or add/remove address)",
                   BooleanValue (false),
                   MakeBooleanAccessor (&Ipv4GlobalRouting::m_respondToInterfaceEvents),
                   MakeBooleanChecker ())
  ;
  return tid;
}

Ipv4GlobalRouting::Ipv4GlobalRouting () 
  : m_randomEcmpRouting (false),
    m_respondToInterfaceEvents (false)
{
  NS_LOG_FUNCTION_NOARGS ();
}

Ipv4GlobalRouting::~Ipv4GlobalRouting ()
{
  NS_LOG_FUNCTION_NOARGS ();
}

void 
Ipv4GlobalRouting::AddHostRouteTo (Ipv4Address dest, 
                                   Ipv4Address nextHop, 
                                   uint32_t interface)
{
  NS_LOG_FUNCTION (dest << nextHop << interface);
  Ipv4RoutingTableEntry *route = new Ipv4RoutingTableEntry ();
  *route = Ipv4RoutingTableEntry::CreateHostRouteTo (dest, nextHop, interface);
  m_hostRoutes.push_back (route);
  m_outgoingInterfaces[dest].push_back(interface);
  if (m_stateMachines.find(dest) == m_stateMachines.end()) {
    m_stateMachines[dest] = std::vector<DdcState>(m_ipv4->GetNInterfaces());
    for (int i = 0; i < (int)m_ipv4->GetNInterfaces(); i++) {
      m_stateMachines[dest][i] = Input;
    }
  }
  m_stateMachines[dest][interface] = Output;
}

void 
Ipv4GlobalRouting::AddHostRouteTo (Ipv4Address dest, 
                                   uint32_t interface)
{
  NS_LOG_FUNCTION (dest << interface);
  Ipv4RoutingTableEntry *route = new Ipv4RoutingTableEntry ();
  *route = Ipv4RoutingTableEntry::CreateHostRouteTo (dest, interface);
  m_hostRoutes.push_back (route);
  m_outgoingInterfaces[dest].push_back(interface);
  if (m_stateMachines.find(dest) == m_stateMachines.end()) {
    m_stateMachines[dest] = std::vector<DdcState>(m_ipv4->GetNInterfaces());
    for (int i = 0; i < (int)m_ipv4->GetNInterfaces(); i++) {
      m_stateMachines[dest][i] = Input;
    }
  }
  m_stateMachines[dest][interface] = Output;
}

void 
Ipv4GlobalRouting::AddNetworkRouteTo (Ipv4Address network, 
                                      Ipv4Mask networkMask, 
                                      Ipv4Address nextHop, 
                                      uint32_t interface)
{
  NS_LOG_FUNCTION (network << networkMask << nextHop << interface);
  Ipv4RoutingTableEntry *route = new Ipv4RoutingTableEntry ();
  *route = Ipv4RoutingTableEntry::CreateNetworkRouteTo (network,
                                                        networkMask,
                                                        nextHop,
                                                        interface);
  m_networkRoutes.push_back (route);
  m_outgoingInterfaces[network].push_back(interface);
  if (m_stateMachines.find(network) == m_stateMachines.end()) {
    m_stateMachines[network] = std::vector<DdcState>(m_ipv4->GetNInterfaces());
    for (int i = 0; i < (int)m_ipv4->GetNInterfaces(); i++) {
      m_stateMachines[network][i] = Input;
    }
  }
  m_stateMachines[network][interface] = Output;
}

void 
Ipv4GlobalRouting::AddNetworkRouteTo (Ipv4Address network, 
                                      Ipv4Mask networkMask, 
                                      uint32_t interface)
{
  NS_LOG_FUNCTION (network << networkMask << interface);
  Ipv4RoutingTableEntry *route = new Ipv4RoutingTableEntry ();
  *route = Ipv4RoutingTableEntry::CreateNetworkRouteTo (network,
                                                        networkMask,
                                                        interface);
  m_networkRoutes.push_back (route);
  m_outgoingInterfaces[network].push_back(interface);
  if (m_stateMachines.find(network) == m_stateMachines.end()) {
    m_stateMachines[network] = std::vector<DdcState>(m_ipv4->GetNInterfaces());
    for (int i = 0; i < (int)m_ipv4->GetNInterfaces(); i++) {
      m_stateMachines[network][i] = Input;
    }
  }
  m_stateMachines[network][interface] = Output;
}

void 
Ipv4GlobalRouting::AddASExternalRouteTo (Ipv4Address network, 
                                         Ipv4Mask networkMask,
                                         Ipv4Address nextHop,
                                         uint32_t interface)
{
  NS_LOG_FUNCTION (network << networkMask << nextHop);
  Ipv4RoutingTableEntry *route = new Ipv4RoutingTableEntry ();
  *route = Ipv4RoutingTableEntry::CreateNetworkRouteTo (network,
                                                        networkMask,
                                                        nextHop,
                                                        interface);
  m_ASexternalRoutes.push_back (route);
  m_outgoingInterfaces[network].push_back(interface);
}

Ipv4GlobalRouting::RouteVec_t 
Ipv4GlobalRouting::FindEqualCostPaths (Ipv4Address dest, Ptr<NetDevice> oif)
{
  NS_LOG_FUNCTION_NOARGS ();
  NS_LOG_LOGIC ("Looking for route for destination " << dest);
  RouteVec_t allRoutes;

  NS_LOG_LOGIC ("Number of m_hostRoutes = " << m_hostRoutes.size ());
  for (HostRoutesCI i = m_hostRoutes.begin (); 
       i != m_hostRoutes.end (); 
       i++) 
    {
      NS_ASSERT ((*i)->IsHost ());
      if ((*i)->GetDest ().IsEqual (dest)) 
        {
          if (oif != 0)
            {
              if (oif != m_ipv4->GetNetDevice ((*i)->GetInterface ()))
                {
                  NS_LOG_LOGIC ("Not on requested interface, skipping");
                  continue;
                }
            }
          allRoutes.push_back (*i);
          NS_LOG_LOGIC (allRoutes.size () << " Found global host route " << *i); 
          NS_LOG_LOGIC(dest << " can use " << m_outgoingInterfaces[dest].size() << " interfaces "); 
        }
    }
  if (allRoutes.size () == 0) // if no host route is found
    {
      NS_LOG_LOGIC (" Number of m_networkRoutes" << m_networkRoutes.size ());
      for (NetworkRoutesI j = m_networkRoutes.begin (); 
           j != m_networkRoutes.end (); 
           j++) 
        {
          Ipv4Mask mask = (*j)->GetDestNetworkMask ();
          Ipv4Address entry = (*j)->GetDestNetwork ();
          if (mask.IsMatch (dest, entry)) 
            {
              if (oif != 0)
                {
                  if (oif != m_ipv4->GetNetDevice ((*j)->GetInterface ()))
                    {
                      NS_LOG_LOGIC ("Not on requested interface, skipping");
                      continue;
                    }
                }
              allRoutes.push_back (*j);
              NS_LOG_LOGIC (allRoutes.size () << "Found global network route" << *j);
              NS_LOG_LOGIC(dest << " can use " << m_outgoingInterfaces[entry].size() << " interfaces "); 
            }
        }
    }
  if (allRoutes.size () == 0)  // consider external if no host/network found
    {
      for (ASExternalRoutesI k = m_ASexternalRoutes.begin ();
           k != m_ASexternalRoutes.end ();
           k++)
        {
          Ipv4Mask mask = (*k)->GetDestNetworkMask ();
          Ipv4Address entry = (*k)->GetDestNetwork ();
          if (mask.IsMatch (dest, entry))
            {
              NS_LOG_LOGIC ("Found external route" << *k);
              if (oif != 0)
                {
                  if (oif != m_ipv4->GetNetDevice ((*k)->GetInterface ()))
                    {
                      NS_LOG_LOGIC ("Not on requested interface, skipping");
                      continue;
                    }
                }
              allRoutes.push_back (*k);
              break;
            }
        }
    }
  return allRoutes;
}

Ptr<Ipv4Route>
Ipv4GlobalRouting::LookupGlobal (Ipv4Address dest, Ptr<NetDevice> oif)
{
  Ptr<Ipv4Route> rtentry = 0;
  Ipv4GlobalRouting::RouteVec_t allRoutes = FindEqualCostPaths(dest, oif);
  if (allRoutes.size () > 0 ) // if route(s) is found
    {
      // pick up one of the routes uniformly at random if random
      // ECMP routing is enabled, or always select the first route
      // consistently if random ECMP routing is disabled
      uint32_t selectIndex;
      if (m_randomEcmpRouting)
        {
          selectIndex = m_rand.GetInteger (0, allRoutes.size ()-1);
        }
      else 
        {
          selectIndex = 0;
        }
      Ipv4RoutingTableEntry* route = allRoutes.at (selectIndex); 
      // create a Ipv4Route object from the selected routing table entry
      rtentry = Create<Ipv4Route> ();
      rtentry->SetDestination (route->GetDest ());
      // XXX handle multi-address case
      rtentry->SetSource (m_ipv4->GetAddress (route->GetInterface (), 0).GetLocal ());
      rtentry->SetGateway (route->GetGateway ());
      uint32_t interfaceIdx = route->GetInterface ();
      rtentry->SetOutputDevice (m_ipv4->GetNetDevice (interfaceIdx));
      return rtentry;
    }
  else 
    {
      return 0;
    }
}

uint32_t 
Ipv4GlobalRouting::GetNRoutes (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  uint32_t n = 0;
  n += m_hostRoutes.size ();
  n += m_networkRoutes.size ();
  n += m_ASexternalRoutes.size ();
  return n;
}

Ipv4RoutingTableEntry *
Ipv4GlobalRouting::GetRoute (uint32_t index) const
{
  NS_LOG_FUNCTION (index);
  if (index < m_hostRoutes.size ())
    {
      uint32_t tmp = 0;
      for (HostRoutesCI i = m_hostRoutes.begin (); 
           i != m_hostRoutes.end (); 
           i++) 
        {
          if (tmp  == index)
            {
              return *i;
            }
          tmp++;
        }
    }
  index -= m_hostRoutes.size ();
  uint32_t tmp = 0;
  if (index < m_networkRoutes.size ())
    {
      for (NetworkRoutesCI j = m_networkRoutes.begin (); 
           j != m_networkRoutes.end ();
           j++)
        {
          if (tmp == index)
            {
              return *j;
            }
          tmp++;
        }
    }
  index -= m_networkRoutes.size ();
  tmp = 0;
  for (ASExternalRoutesCI k = m_ASexternalRoutes.begin (); 
       k != m_ASexternalRoutes.end (); 
       k++) 
    {
      if (tmp == index)
        {
          return *k;
        }
      tmp++;
    }
  NS_ASSERT (false);
  // quiet compiler.
  return 0;
}
void 
Ipv4GlobalRouting::RemoveRoute (uint32_t index)
{
  NS_LOG_FUNCTION (index);
  if (index < m_hostRoutes.size ())
    {
      uint32_t tmp = 0;
      for (HostRoutesI i = m_hostRoutes.begin (); 
           i != m_hostRoutes.end (); 
           i++) 
        {
          if (tmp  == index)
            {
              NS_LOG_LOGIC ("Removing route " << index << "; size = " << m_hostRoutes.size ());
              delete *i;
              m_hostRoutes.erase (i);
              NS_LOG_LOGIC ("Done removing host route " << index << "; host route remaining size = " << m_hostRoutes.size ());
              return;
            }
          tmp++;
        }
    }
  index -= m_hostRoutes.size ();
  uint32_t tmp = 0;
  for (NetworkRoutesI j = m_networkRoutes.begin (); 
       j != m_networkRoutes.end (); 
       j++) 
    {
      if (tmp == index)
        {
          NS_LOG_LOGIC ("Removing route " << index << "; size = " << m_networkRoutes.size ());
          delete *j;
          m_networkRoutes.erase (j);
          NS_LOG_LOGIC ("Done removing network route " << index << "; network route remaining size = " << m_networkRoutes.size ());

          return;
        }
      tmp++;
    }
  index -= m_networkRoutes.size ();
  tmp = 0;
  for (ASExternalRoutesI k = m_ASexternalRoutes.begin (); 
       k != m_ASexternalRoutes.end ();
       k++)
    {
      if (tmp == index)
        {
          NS_LOG_LOGIC ("Removing route " << index << "; size = " << m_ASexternalRoutes.size ());
          delete *k;
          m_ASexternalRoutes.erase (k);
          NS_LOG_LOGIC ("Done removing network route " << index << "; network route remaining size = " << m_networkRoutes.size ());
          return;
        }
      tmp++;
    }
  NS_ASSERT (false);
}

void
Ipv4GlobalRouting::DoDispose (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  for (HostRoutesI i = m_hostRoutes.begin (); 
       i != m_hostRoutes.end (); 
       i = m_hostRoutes.erase (i)) 
    {
      delete (*i);
    }
  for (NetworkRoutesI j = m_networkRoutes.begin (); 
       j != m_networkRoutes.end (); 
       j = m_networkRoutes.erase (j)) 
    {
      delete (*j);
    }
  for (ASExternalRoutesI l = m_ASexternalRoutes.begin (); 
       l != m_ASexternalRoutes.end ();
       l = m_ASexternalRoutes.erase (l))
    {
      delete (*l);
    }

  Ipv4RoutingProtocol::DoDispose ();
}

// Formatted like output of "route -n" command
void
Ipv4GlobalRouting::PrintRoutingTable (Ptr<OutputStreamWrapper> stream) const
{
  std::ostream* os = stream->GetStream ();
  if (GetNRoutes () > 0)
    {
      *os << "Destination     Gateway         Genmask         Flags Metric Ref    Use Iface" << std::endl;
      for (uint32_t j = 0; j < GetNRoutes (); j++)
        {
          std::ostringstream dest, gw, mask, flags;
          Ipv4RoutingTableEntry route = GetRoute (j);
          dest << route.GetDest ();
          *os << std::setiosflags (std::ios::left) << std::setw (16) << dest.str ();
          gw << route.GetGateway ();
          *os << std::setiosflags (std::ios::left) << std::setw (16) << gw.str ();
          mask << route.GetDestNetworkMask ();
          *os << std::setiosflags (std::ios::left) << std::setw (16) << mask.str ();
          flags << "U";
          if (route.IsHost ())
            {
              flags << "H";
            }
          else if (route.IsGateway ())
            {
              flags << "G";
            }
          *os << std::setiosflags (std::ios::left) << std::setw (6) << flags.str ();
          // Metric not implemented
          *os << "-" << "      ";
          // Ref ct not implemented
          *os << "-" << "      ";
          // Use not implemented
          *os << "-" << "   ";
          if (Names::FindName (m_ipv4->GetNetDevice (route.GetInterface ())) != "")
            {
              *os << Names::FindName (m_ipv4->GetNetDevice (route.GetInterface ()));
            }
          else
            {
              *os << route.GetInterface ();
            }
          *os << std::endl;
        }
    }
}

Ptr<Ipv4Route>
Ipv4GlobalRouting::RouteOutput (Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr)
{

//
// First, see if this is a multicast packet we have a route for.  If we
// have a route, then send the packet down each of the specified interfaces.
//
  if (header.GetDestination ().IsMulticast ())
    {
      NS_LOG_LOGIC ("Multicast destination-- returning false");
      return 0; // Let other routing protocols try to handle this
    }
//
// See if this is a unicast packet we have a route for.
//
  NS_LOG_LOGIC ("Unicast destination- looking up");
  Ptr<Ipv4Route> rtentry = LookupGlobal (header.GetDestination (), oif);
  if (rtentry)
    {
      sockerr = Socket::ERROR_NOTERROR;
    }
  else
    {
      sockerr = Socket::ERROR_NOROUTETOHOST;
    }
  return rtentry;
}

bool 
Ipv4GlobalRouting::RouteInput  (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev,                             UnicastForwardCallback ucb, MulticastForwardCallback mcb,
                                LocalDeliverCallback lcb, ErrorCallback ecb)
{ 

  NS_LOG_FUNCTION (this << p << header << header.GetSource () << header.GetDestination () << idev);
  // Check if input device supports IP
  NS_ASSERT (m_ipv4->GetInterfaceForDevice (idev) >= 0);
  uint32_t iif = m_ipv4->GetInterfaceForDevice (idev);

  if (header.GetDestination ().IsMulticast ())
    {
      NS_LOG_LOGIC ("Multicast destination-- returning false");
      return false; // Let other routing protocols try to handle this
    }

  if (header.GetDestination ().IsBroadcast ())
    {
      NS_LOG_LOGIC ("For me (Ipv4Addr broadcast address)");
      // TODO:  Local Deliver for broadcast
      // TODO:  Forward broadcast
    }

  // TODO:  Configurable option to enable RFC 1222 Strong End System Model
  // Right now, we will be permissive and allow a source to send us
  // a packet to one of our other interface addresses; that is, the
  // destination unicast address does not match one of the iif addresses,
  // but we check our other interfaces.  This could be an option
  // (to remove the outer loop immediately below and just check iif).
  for (uint32_t j = 0; j < m_ipv4->GetNInterfaces (); j++)
    {
      
      for (uint32_t i = 0; i < m_ipv4->GetNAddresses (j); i++)
        {
          Ipv4InterfaceAddress iaddr = m_ipv4->GetAddress (j, i);
          Ipv4Address addr = iaddr.GetLocal ();
          if (addr.IsEqual (header.GetDestination ()))
            {
              if (j == iif)
                {
                  NS_LOG_LOGIC ("For me (destination " << addr << " match)");
                }
              else
                {
                  NS_LOG_LOGIC ("For me (destination " << addr << " match) on another interface " << header.GetDestination ());
                }
              lcb (p, header, iif);
              return true;
            }
          if (header.GetDestination ().IsEqual (iaddr.GetBroadcast ()))
            {
              NS_LOG_LOGIC ("For me (interface broadcast address)");
              lcb (p, header, iif);
              return true;
            }
          NS_LOG_LOGIC ("Address "<< addr << " not a match");
        }
    }
  // Check if input device supports IP forwarding
  if (m_ipv4->IsForwarding (iif) == false)
    {
      NS_LOG_LOGIC ("Forwarding disabled for this interface");
      ecb (p, header, Socket::ERROR_NOROUTETOHOST);
      return false;
    }

  NS_LOG_LOGIC("state machine = " << 
               m_stateMachines[header.GetDestination()][idev->GetIfIndex()] << 
               " dev = " << idev->GetIfIndex() <<
               " dest = " << header.GetDestination());

  // Next, try to find a route
  NS_LOG_LOGIC ("Unicast destination- looking up global route");
  Ptr<Ipv4Route> rtentry = LookupGlobal (header.GetDestination ());
  if (rtentry != 0)
    {
      NS_LOG_LOGIC ("Found unicast destination- calling unicast callback");
      ucb (rtentry, p, header);
      return true;
    }
  else
    {
      NS_LOG_LOGIC ("Did not find unicast destination- returning false");
      return false; // Let other routing protocols try to handle this
                    // route request.
    }
}
void 
Ipv4GlobalRouting::NotifyInterfaceUp (uint32_t i)
{
  NS_LOG_FUNCTION (this << i);
  if (m_respondToInterfaceEvents && Simulator::Now ().GetSeconds () > 0)  // avoid startup events
    {
      GlobalRouteManager::DeleteGlobalRoutes ();
      GlobalRouteManager::BuildGlobalRoutingDatabase ();
      GlobalRouteManager::InitializeRoutes ();
    }
}

void 
Ipv4GlobalRouting::NotifyInterfaceDown (uint32_t i)
{
  NS_LOG_FUNCTION (this << i);
  if (m_respondToInterfaceEvents && Simulator::Now ().GetSeconds () > 0)  // avoid startup events
    {
      GlobalRouteManager::DeleteGlobalRoutes ();
      GlobalRouteManager::BuildGlobalRoutingDatabase ();
      GlobalRouteManager::InitializeRoutes ();
    }
}

void 
Ipv4GlobalRouting::NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address)
{
  NS_LOG_FUNCTION (this << interface << address);
  if (m_respondToInterfaceEvents && Simulator::Now ().GetSeconds () > 0)  // avoid startup events
    {
      GlobalRouteManager::DeleteGlobalRoutes ();
      GlobalRouteManager::BuildGlobalRoutingDatabase ();
      GlobalRouteManager::InitializeRoutes ();
    }
}

void 
Ipv4GlobalRouting::NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address)
{
  NS_LOG_FUNCTION (this << interface << address);
  if (m_respondToInterfaceEvents && Simulator::Now ().GetSeconds () > 0)  // avoid startup events
    {
      GlobalRouteManager::DeleteGlobalRoutes ();
      GlobalRouteManager::BuildGlobalRoutingDatabase ();
      GlobalRouteManager::InitializeRoutes ();
    }
}

void 
Ipv4GlobalRouting::SetIpv4 (Ptr<Ipv4> ipv4)
{
  NS_LOG_FUNCTION (this << ipv4);
  NS_ASSERT (m_ipv4 == 0 && ipv4 != 0);
  m_ipv4 = ipv4;
}


} // namespace ns3
