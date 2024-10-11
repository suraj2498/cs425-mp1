/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Definition of MP1Node class functions.
 **********************************/

#include "MP1Node.h"
#include "Member.h"

/*
 * Note: You can change/add any functions in MP1Node.{h,cpp}
 */

/**
 * Overloaded Constructor of the MP1Node class
 * You can add new members to the class if you think it
 * is necessary for your logic to work
 */
MP1Node::MP1Node(Params *params, EmulNet *emul, Log *log, Address *address) {
  for (int i = 0; i < 6; i++) {
    NULLADDR[i] = 0;
  }
  this->memberNode = new Member;
  this->shouldDeleteMember = true;
  memberNode->inited = false;
  this->emulNet = emul;
  this->log = log;
  this->par = params;
  this->memberNode->addr = *address;
}

/**
 * Overloaded Constructor of the MP1Node class
 * You can add new members to the class if you think it
 * is necessary for your logic to work
 */
MP1Node::MP1Node(Member *member, Params *params, EmulNet *emul, Log *log,
                 Address *address) {
  for (int i = 0; i < 6; i++) {
    NULLADDR[i] = 0;
  }
  this->memberNode = member;
  this->shouldDeleteMember = false;
  this->emulNet = emul;
  this->log = log;
  this->par = params;
  this->memberNode->addr = *address;
}

/**
 * Destructor of the MP1Node class
 */
MP1Node::~MP1Node() {
  if (shouldDeleteMember) {
    delete this->memberNode;
  }
}

/**
 * FUNCTION NAME: recvLoop
 *
 * DESCRIPTION: This function receives message from the network and pushes into
 * the queue This function is called by a node to receive messages currently
 * waiting for it
 */
int MP1Node::recvLoop() {
  if (memberNode->bFailed) {
    return false;
  } else {
    return emulNet->ENrecv(&(memberNode->addr), enqueueWrapper, NULL, 1,
                           &(memberNode->mp1q));
  }
}

/**
 * FUNCTION NAME: enqueueWrapper
 *
 * DESCRIPTION: Enqueue the message from Emulnet into the queue
 */
int MP1Node::enqueueWrapper(void *env, char *buff, int size) {
  Queue q;
  return q.enqueue((queue<q_elt> *)env, (void *)buff, size);
}

/**
 * FUNCTION NAME: nodeStart
 *
 * DESCRIPTION: This function bootstraps the node
 * 				All initializations routines for a member.
 * 				Called by the application layer.
 */
void MP1Node::nodeStart(char *servaddrstr, short servport) {
  Address joinaddr;
  joinaddr = getJoinAddress();

  // Self booting routines
  if (initThisNode(&joinaddr) == -1) {
#ifdef DEBUGLOG
    log->LOG(&memberNode->addr, "init_thisnode failed. Exit.");
#endif
    exit(1);
  }

  if (!introduceSelfToGroup(&joinaddr)) {
    finishUpThisNode();
#ifdef DEBUGLOG
    log->LOG(&memberNode->addr, "Unable to join self to group. Exiting.");
#endif
    exit(1);
  }

  return;
}

/**
 * FUNCTION NAME: initThisNode
 *
 * DESCRIPTION: Find out who I am and start up
 */
int MP1Node::initThisNode(Address *joinaddr) {
  memberNode->bFailed = false;
  memberNode->inited = true;
  memberNode->inGroup = false;
  // node is up!
  memberNode->nnb = 0;
  memberNode->heartbeat = 0;
  memberNode->pingCounter = TFAIL;
  memberNode->timeOutCounter = -1;
  initMemberListTable(memberNode);

  return 0;
}

/**
 * FUNCTION NAME: introduceSelfToGroup
 *
 * DESCRIPTION: Join the distributed system
 */
int MP1Node::introduceSelfToGroup(Address *joinaddr) {
  MessageHdr *msg;
#ifdef DEBUGLOG
  static char s[1024];
#endif

  if (0 ==
      strcmp((char *)&(memberNode->addr.addr), (char *)&(joinaddr->addr))) {
    // I am the group booter (first process to join the group). Boot up the
    // group
#ifdef DEBUGLOG
    log->LOG(&memberNode->addr, "Starting up group...");
#endif
    memberNode->inGroup = true;
    // Construct a new MemberListEntry using the node's address and heartbeat
    MemberListEntry newMemberEntry((int)(memberNode->addr.addr[0]),
                                   (short)(memberNode->addr.addr[4]),
                                   memberNode->heartbeat, par->getcurrtime());

    // Add the newly created entry to the member list
    memberNode->memberList.emplace_back(newMemberEntry);

    // Initialize the Address for logging the node join event
    Address loggedNodeAddr = initMemberAddress(newMemberEntry);
    log->logNodeAdd(&memberNode->addr, &loggedNodeAddr);
  } else {
    size_t msgsize =
        sizeof(MessageHdr) + (sizeof(memberNode->addr.addr)) + sizeof(long) + 1;
    msg = (MessageHdr *)malloc(msgsize * sizeof(char));

    // create JOINREQ message: format of data is {struct Address myaddr}
    msg->msgType = JOINREQ;
    memcpy((char *)(msg + 1), &memberNode->addr.addr,
           sizeof(memberNode->addr.addr));
    memcpy((char *)(msg + 1) + 1 + sizeof(memberNode->addr.addr),
           &memberNode->heartbeat, sizeof(long));

#ifdef DEBUGLOG
    sprintf(s, "Trying to join...");
    log->LOG(&memberNode->addr, s);
#endif

    // send JOINREQ message to introducer member
    emulNet->ENsend(&memberNode->addr, joinaddr, (char *)msg, msgsize);
    free(msg);
  }

  return 1;
}

/**
 * FUNCTION NAME: finishUpThisNode
 *
 * DESCRIPTION: Wind up this node and clean up state
 */
int MP1Node::finishUpThisNode() {
  memberNode = new Member();
  return 0;
}

/**
 * FUNCTION NAME: nodeLoop
 *
 * DESCRIPTION: Executed periodically at each member
 * 				Check your messages in queue and perform
 * membership protocol duties
 */
void MP1Node::nodeLoop() {
  if (memberNode->bFailed) {
    return;
  }

  // Check my messages
  checkMessages();

  // register the hearbeat
  memberNode->heartbeat += 1;

  // Prepare a pointer to iterate through the member list
  auto *listPtr = memberNode->memberList.data();
  int totalEntries = memberNode->memberList.size();
  int position = 0;

  while (position < totalEntries) {
    // Check if the current entry corresponds to this node
    if (listPtr[position].id == (int)(memberNode->addr.addr[0])) {
      // Update heartbeat and timestamp for the matched entry
      listPtr[position].setheartbeat(memberNode->heartbeat);
      listPtr[position].settimestamp(par->getcurrtime());
      break;
    }
    position++;
  }

  // Wait until you're in the group...
  if (!memberNode->inGroup) {
    return;
  }

  // ...then jump in and share your responsibilites!
  nodeLoopOps();

  return;
}

/**
 * FUNCTION NAME: checkMessages
 *
 * DESCRIPTION: Check messages in the queue and call the respective message
 * handler
 */
void MP1Node::checkMessages() {
  void *ptr;
  int size;

  // Pop waiting messages from memberNode's mp1q
  while (!memberNode->mp1q.empty()) {
    ptr = memberNode->mp1q.front().elt;
    size = memberNode->mp1q.front().size;
    memberNode->mp1q.pop();
    recvCallBack((void *)memberNode, (char *)ptr, size);
  }
  return;
}

/**
 * FUNCTION NAME: recvCallBack
 *
 * DESCRIPTION: Message handler for different message types
 */
bool MP1Node::recvCallBack(void *env, char *data, int size) {
  vector<MemberListEntry> incomingMemberList;
  Address senderAddr = Address();
  long heartbeatValue;
  MessageHdr *incomingMsg;
  int offset = 0;

  incomingMsg = (MessageHdr *)data;
  memcpy(&senderAddr.addr, data + sizeof(MessageHdr),
         sizeof(memberNode->addr.addr));
  offset += sizeof(memberNode->addr.addr) + 1;
  memcpy(&heartbeatValue, (data + sizeof(MessageHdr)) + offset, sizeof(long));
  offset += sizeof(long);

  if (incomingMsg->msgType == JOINREQ) {
    int memberListSize = memberNode->memberList.size();
    size_t totalMsgSize = sizeof(MessageHdr) + sizeof(memberNode->addr.addr) +
                          sizeof(long) + sizeof(int) +
                          memberListSize * MEMBER_SIZE;

    // Allocate memory for the message
    MessageHdr *message = (MessageHdr *)malloc(totalMsgSize + 1);
    if (!message) {
      log->LOG(&memberNode->addr, "Memory allocation failed for JOINREP.");
    }

    message->msgType = JOINREP;

    // Copy the sender node's address to the message
    char *dataPtr = (char *)(message + 1);
    memcpy(dataPtr, memberNode->addr.addr, sizeof(memberNode->addr.addr));
    dataPtr += sizeof(memberNode->addr.addr) + 1;
    // Copy the node's heartbeat value into the message
    memcpy(dataPtr, &memberNode->heartbeat, sizeof(long));
    dataPtr += sizeof(long);

    serializeMemberData(dataPtr);

    // Send the JOINREP message to the receiver node
    emulNet->ENsend(&memberNode->addr, &senderAddr, (char *)message,
                    totalMsgSize);
    free(message);
    log->logNodeAdd(&memberNode->addr, &senderAddr);
  } else if (incomingMsg->msgType == JOINREP) {
    // join the group
    memberNode->inGroup = true;
    memberNode->inited = true;
    memberNode->memberList.emplace_back(MemberListEntry(
        int(memberNode->addr.addr[0]), short(memberNode->addr.addr[4]),
        memberNode->heartbeat, this->par->getcurrtime()));

    processMemberData(data + sizeof(MessageHdr) + offset, incomingMemberList);
  } else if (incomingMsg->msgType == GOSSIP) {
    processMemberData(data + sizeof(MessageHdr) + offset, incomingMemberList);
  }

  return true;
}

/**
 * FUNCTION NAME: nodeLoopOps
 *
 * DESCRIPTION: Check if any node hasn't responded within a timeout period and
 * then delete the nodes Propagate your membership list
 */
void MP1Node::nodeLoopOps() {
  std::vector<MemberListEntry> tempMemberList;
  int memberListSize = memberNode->memberList.size();
  size_t msgSize = sizeof(MessageHdr) + sizeof(memberNode->addr.addr) + 1 +
                   sizeof(long) + sizeof(int) + memberListSize * MEMBER_SIZE;
  MessageHdr *msg = (MessageHdr *)malloc(msgSize);

  if (msg == nullptr) {
    log->LOG(&memberNode->addr, "Memory allocation failed for GOSSIP message.");
    return;
  }

  // Iterate over the current member list and selectively copy non-stale members
  for (auto &entry : memberNode->memberList) {
    // If the member's timestamp is within the valid range, add to the temporary
    // list
    if (entry.gettimestamp() >= this->par->getcurrtime() - TREMOVE) {
      tempMemberList.emplace_back(entry);
    } else {
      // Log the removal of stale nodes
      Address outdatedNodeAddr = initMemberAddress(entry);
      log->logNodeRemove(&memberNode->addr, &outdatedNodeAddr);
    }
  }

  // Clear the current member list and repopulate it with non-stale entries
  memberNode->memberList.clear();
  for (const auto &newEntry : tempMemberList) {
    memberNode->memberList.emplace_back(newEntry);
  }

  // send the gossip messages
  msg->msgType = GOSSIP;
  char *msgPtr = (char *)(msg + 1);
  memcpy(msgPtr, &memberNode->addr.addr, sizeof(memberNode->addr.addr));
  msgPtr += sizeof(memberNode->addr.addr) + 1;

  // Copy the heartbeat into the message
  memcpy(msgPtr, &memberNode->heartbeat, sizeof(long));
  msgPtr += sizeof(long);
  serializeMemberData(msgPtr);

  // Iterate over the member list and send the GOSSIP message to all members
  // except self
  for (auto &memberEntry : memberNode->memberList) {
    // Skip if the destination address is the current node itself
    Address destAddress = initMemberAddress(memberEntry);
    if (!(memberNode->addr == destAddress)) {
      emulNet->ENsend(&memberNode->addr, &destAddress, (char *)msg, msgSize);
    }
  }

  free(msg);
  return;
}

/**
 * FUNCTION NAME: isNullAddress
 *
 * DESCRIPTION: Function checks if the address is NULL
 */
int MP1Node::isNullAddress(Address *addr) {
  return (memcmp(addr->addr, NULLADDR, 6) == 0 ? 1 : 0);
}

/**
 * FUNCTION NAME: getJoinAddress
 *
 * DESCRIPTION: Returns the Address of the coordinator
 */
Address MP1Node::getJoinAddress() {
  Address joinaddr{};

  *(int *)(&joinaddr.addr) = 1;
  *(short *)(&joinaddr.addr[4]) = 0;

  return joinaddr;
}

/**
 * FUNCTION NAME: initMemberListTable
 *
 * DESCRIPTION: Initialize the membership list
 */
void MP1Node::initMemberListTable(Member *memberNode) {
  memberNode->memberList.clear();
}

/**
 * FUNCTION NAME: printAddress
 *
 * DESCRIPTION: Print the Address
 */
void MP1Node::printAddress(Address *addr) {
  printf("%d.%d.%d.%d:%d \n", addr->addr[0], addr->addr[1], addr->addr[2],
         addr->addr[3], *(short *)&addr->addr[4]);
}

// HELPER FUNCTIONS

Address MP1Node::initMemberAddress(MemberListEntry entry) {
  Address address = Address();
  address.init();
  address.addr[0] = entry.id;
  address.addr[4] = entry.port;
  return address;
}

void MP1Node::processMemberData(char *msg,
                                std::vector<MemberListEntry> &memberList) {
  // Deserialize the incoming member list data
  deserializeMemberData(msg, memberList);

  // Merge the remote member list with the local one
  mergeMemberLists(memberList);

  // Update the neighbor count (excluding self)
  memberNode->nnb = memberNode->memberList.size() - 1;
}

void MP1Node::mergeMemberLists(vector<MemberListEntry> &memberList) {
  for (auto &incomingEntry : memberList) {
    bool entryExists = false;

    for (size_t i = 0; i < memberNode->memberList.size(); ++i) {
      MemberListEntry &currentEntry = memberNode->memberList[i];

      if (currentEntry.id == incomingEntry.id) {
        entryExists = true;
        // Update heartbeat and timestamp if remote entry has a fresher
        // heartbeat
        if (currentEntry.heartbeat < incomingEntry.heartbeat) {
          currentEntry.setheartbeat(incomingEntry.heartbeat);
          currentEntry.settimestamp(par->getcurrtime());
        }
        break;
      }
    }

    // If no matching entry was found, add the incoming entry to the local list
    if (!entryExists &&
        incomingEntry.gettimestamp() >= par->getcurrtime() - TFAIL) {
      // Create new member entry using the provided method for consistency
      MemberListEntry newMemberEntry(
          (int)(incomingEntry.id), (short)(incomingEntry.port),
          incomingEntry.heartbeat, par->getcurrtime());

      memberNode->memberList.emplace_back(newMemberEntry);

      Address newlyJoined = initMemberAddress(newMemberEntry);
      log->logNodeAdd(&memberNode->addr, &newlyJoined);
    }
  }
}

void MP1Node::serializeMemberData(char *msg) {
  std::map<int, MemberListEntry> memberMap;

  // Populate the map from the current member list
  for (const auto &entry : memberNode->memberList) {
    memberMap[entry.id] = entry;
  }

  // Use a single pointer for ref to the message
  char *msgPtr = msg;

  // store the size of the list at the start
  *(int *)msgPtr = (int)(memberMap.size());
  msgPtr += sizeof(int);

  // Iterate through the map and store each member entry inline
  for (const auto &pair : memberMap) {
    const MemberListEntry &entry = pair.second;
    *(int *)msgPtr = (int)entry.id;
    msgPtr += sizeof(int);
    *(short *)msgPtr = (short)entry.port;
    msgPtr += sizeof(short);
    *(long *)msgPtr = (long)entry.heartbeat;
    msgPtr += sizeof(long);
    *(long *)msgPtr = (long)entry.timestamp;
    msgPtr += sizeof(long);
  }
}

void MP1Node::deserializeMemberData(
    char *msg, std::vector<MemberListEntry> &receivedMemberList) {
  // Use a pointer to traverse the buffer
  char *msgPtr = msg;

  // Extract the number of members (list size)
  int memberCount = *(int *)msgPtr;
  msgPtr += sizeof(int);

  for (int i = 0; i < memberCount; i++) {
    int memberId = *(int *)msgPtr;
    msgPtr += sizeof(int);
    short memberPort = *(short *)msgPtr;
    msgPtr += sizeof(short);
    long memberHeartbeat = *(long *)msgPtr;
    msgPtr += sizeof(long);
    long memberTimestamp = *(long *)msgPtr;
    msgPtr += sizeof(long);

    receivedMemberList.emplace_back(memberId, memberPort, memberHeartbeat,
                                    memberTimestamp);
  }
}
