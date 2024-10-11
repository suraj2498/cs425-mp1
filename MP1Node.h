/**********************************
 * FILE NAME: MP1Node.h
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Header file of MP1Node class. (Revised 2020)
 *
 *  Starter code template
 **********************************/

#ifndef _MP1NODE_H_
#define _MP1NODE_H_

#include "EmulNet.h"
#include "Log.h"
#include "Member.h"
#include "Params.h"
#include "Queue.h"
#include "stdincludes.h"

/**
 * Macros
 */
#define TREMOVE 20
#define TFAIL 5

/**
 * CLASS NAME: MP1Node
 *
 * DESCRIPTION: Class implementing Membership protocol functionalities for
 * failure detection
 */
class MP1Node {
private:
  EmulNet *emulNet;
  Log *log;
  Params *par;
  Member *memberNode;
  bool shouldDeleteMember;
  char NULLADDR[6];
  static constexpr size_t MEMBER_SIZE = 2 * sizeof(long) + sizeof(int) + sizeof(short);

public:
  /**
   * Message Types
   */
  enum MsgTypes {
    JOINREQ,
    JOINREP,
    UPDATEREQ,
    UPDATEREP,
    DUMMYLASTMSGTYPE,
    GOSSIP
  };

  /**
   * STRUCT NAME: MessageHdr
   *
   * DESCRIPTION: Header and content of a message
   */
  struct MessageHdr {
    MsgTypes msgType;
  };

  MP1Node(Params *params, EmulNet *emul, Log *log, Address *address);
  MP1Node(Member *member, Params *params, EmulNet *emul, Log *log,
          Address *address);
  Member *getMemberNode() { return memberNode; }
  int recvLoop();
  static int enqueueWrapper(void *env, char *buff, int size);
  void nodeStart(char *servaddrstr, short serverport);
  int initThisNode(Address *joinaddr);
  int introduceSelfToGroup(Address *joinAddress);
  int finishUpThisNode();
  void nodeLoop();
  void checkMessages();
  bool recvCallBack(void *env, char *data, int size);
  void nodeLoopOps();
  int isNullAddress(Address *addr);
  Address getJoinAddress();
  void initMemberListTable(Member *memberNode);
  void printAddress(Address *addr);
  virtual ~MP1Node();

  /*
   * Custom helpers
   * */
  Address initMemberAddress(MemberListEntry entry);
  void processMemberData(char *msg, std::vector<MemberListEntry> &memberList);
  void mergeMemberLists(std::vector<MemberListEntry> &remoteMemberList);
  void serializeMemberData(char *msg);
  void deserializeMemberData(char *msg,
                             std::vector<MemberListEntry> &remoteMemberList);
};

#endif /* _MP1NODE_H_ */
