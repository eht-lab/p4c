/* Copyright 2025-2026 EHTech

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#ifndef __TUNA_P4__
#define __TUNA_P4__

/**
 *   P4-16 declaration of the Tunic Architecture
 */

/**********************************************************************
 * Beginning of the part of this target-customized tna.p4 include file
 * that declares data plane widths for one particular target device.
 **********************************************************************/

// BEGIN:Type_defns
/* These are defined using `typedef`, not `type`, so they are truly
 * just different names for the type bit<W> for the particular width W
 * shown.  Unlike the `type` definitions below, values declared with
 * the `typedef` type names can be freely mingled in expressions, just
 * as any value declared with type bit<W> can.  Values declared with
 * one of the `type` names below _cannot_ be so freely mingled, unless
 * you first cast them to the corresponding `typedef` type.  While
 * that may be inconvenient when you need to do arithmetic on such
 * values, it is the price to pay for having all occurrences of values
 * of the `type` types marked as such in the automatically generated
 * control plane API.
 *
 * Note that the width of typedef <name>Uint_t will always be the same
 * as the width of type <name>_t. */

typedef bit<32> PortIdUint_t;
typedef bit<16> PacketLengthUint_t;
typedef bit<32> MulticastGroupUint_t;
typedef bit<16> CloneSessionIdUint_t;
typedef bit<8>  ClassOfServiceUint_t;
typedef bit<16> EgressInstanceUint_t;
typedef bit<64> TimestampUint_t;

type PortIdUint_t         PortId_t;
type PacketLengthUint_t   PacketLength_t;
type MulticastGroupUint_t MulticastGroup_t;
type CloneSessionIdUint_t CloneSessionId_t;
type ClassOfServiceUint_t ClassOfService_t;
type EgressInstanceUint_t EgressInstance_t;
type TimestampUint_t      Timestamp_t;
typedef error   ParserError_t;

const PortId_t TUNA_PORT_RECIRCULATE = (PortId_t) 0xfffffffa;
const CloneSessionId_t TUNA_CLONE_SESSION_TO_CPU = (CloneSessionId_t) 0;

// BEGIN:Hash_algorithms
enum HashAlgorithm {
    csum,            // for checksum
    crc16,           // for checksum
    crc32,           // Standard CRC32 (polynomial 0x04c11db7)
    crc32_1edc6f41,  // Tuna custom CRC32 (polynomial 0x1edc6f41)
    xor4,
    xor8,
    xor16,
    xor32,
    toeplitz         // toeplitz hash
}
// BEGIN:Hash_extern
/// Hash extern for computing hash values with different algorithms
extern Hash<O> {
  /// Constructor: create a Hash instance with specified algorithm
  /// @param algo Hash algorithm to use
  Hash(HashAlgorithm algo);

  /// Compute the hash for data
  /// @param data The data over which to calculate the hash
  /// @return The hash value with type O
  @pure
  O get_hash<D>(in D data);
}
// END:Hash_extern

// BEGIN:Metadata_types
enum TUNA_PacketPath_t {
    // TBD if this type remains, whether it should be an enum or
    // several separate fields representing the same cases in a
    // different form.
    FROM_NET_PORT,
    FROM_NET_LOOPEDBACK,
    FROM_NET_RECIRCULATED, // r2t
    FROM_HOST,
    FROM_HOST_LOOPEDBACK,
    FROM_HOST_RECIRCULATED // t2r
}

struct tuna_ingress_parser_input_metadata_t {
  /*
   * From <-> To desides the packet path
   * From:
   *   dest: 0: VPE, 1: Recircle, 2: ToMPU, 3: ToMPU2 // fix me: ingess/egress have different value of dest
   *   src_engine: 0: NIC, 1:RDMA, 2:ERROR
   *   qtype: 0: normal, 1: req_entry, 2: cmdq_normal, 3: cmdq_test, 4: virtio_sq, 5: virtio_ctrl, 6:error
   * To:
   *   sw_recircle: 0: disable, 1: enable
   *   hw_recircle: 0: disable, 1: enable
   *   toMPU: 0: disable, 1: enable
   *   toMPU2: 0: disable, 1: enable
   *   r2t/t2r: 0: disable, 1: enable
   *   drop: 0: disable, 1: enable
  */
  TUNA_PacketPath_t  packet_path;
  PortId_t           ingress_port;   // network port id
}

struct tuna_egress_parser_input_metadata_t {
  /*
   * From <-> To desides the packet path
   * From:
   *   dest: 0: VPE, 1: swRecircle, 2: hwRecircle, 3: ToMPU, 4: ToMPU2
   *   src_engine: 0: NIC, 1:RDMA, 2:ERROR
   *   qtype: 0: normal, 1: req_entry, 2: cmdq_normal, 3: cmdq_test, 4: virtio_sq, 5: virtio_ctrl, 6:error
   * To:
   *   sw_recircle: 0: disable, 1: enable
   *   hw_recircle: 0: disable, 1: enable
   *   toMPU: 0: disable, 1: enable
   *   toMPU2: 0: disable, 1: enable
   *   r2t/t2r: 0: disable, 1: enable
   *   drop: 0: disable, 1: enable
  */
  TUNA_PacketPath_t  packet_path;
  PortId_t           egress_port;   // virtual port id
}

// BEGIN:Metadata_ingress_input
struct tuna_ingress_input_metadata_t  {
  TUNA_PacketPath_t  packet_path;
  PacketLength_t     packet_length;
  bit<3>             recircle_timestamp;
  PortId_t           ingress_port;
  Timestamp_t        ingress_timestamp;
}

struct tuna_egress_input_metadata_t  {
  TUNA_PacketPath_t  packet_path;
  PacketLength_t     packet_length;
  bit<3>             recircle_timestamp;
  ClassOfService_t   class_of_service;
  PortId_t           egress_port;
  EgressInstance_t   instance;       /// instance comes from the PacketReplicationEngine
  Timestamp_t        egress_timestamp;
}
// END:Metadata_ingress_input

// BEGIN:tuna_output_metadata_t
struct tuna_ingress_output_metadata_t  {
    // Intrstic metadata       bit     comments
    bit<14> len;              // 0~13  the total length of the packet
    bit<1>  drop;             // drop flag
    bit<2>  ecn;
    bit<32> multicast_group;  // multicast group id
    bit<16> clone_session_id; // clone session id
    bit<1>  clone;            // clone flag
    bit<1>  resubmit;         // resubmit flag
    bit<8>  class_of_service; // class of service
    bit<32> port;             // port id
}

struct tuna_egress_output_metadata_t  {
    // Intrstic metadata       bit     comments
    bit<14> len;              // 0~13  the total length of the packet
    bit<1>  drop;             // drop flag
    bit<32> multicast_group;  // multicast group id
    bit<16> clone_session_id; // clone session id
    bit<1>  clone;            // clone flag
    bit<1>  resubmit;         // resubmit flag
    bit<8>  class_of_service; // class of service
    bit<32> port;             // port id
}

// END:Metadata_output

// END:Metadata_types

// BEGIN:Checksum_extern_functions
/***
 * Verifies the checksum of the supplied data.  If this method detects
 * that a checksum of the data is not correct, then the value of the
 * tuna_ingress_input_metadata_t checksum_error field will be equal to 1 when the
 * packet begins ingress processing.
 *
 * Calling verify_checksum is only supported in the VerifyChecksum
 * control.
 *
 * @param T          Must be a tuple type where all the tuple elements
 *                   are of type bit<W>, int<W>, or varbit<W>.  The
 *                   total length of the fields must be a multiple of
 *                   the output size.
 * @param O          Checksum type; must be bit<X> type.
 * @param condition  If 'false' the verification always succeeds.
 * @param data       Data whose checksum is verified.
 * @param checksum   Expected checksum of the data; note that it must
 *                   be a left-value.
 * @param algo       Algorithm to use for checksum (not all algorithms
 *                   may be supported).  Must be a compile-time
 *                   constant.
 */
extern void verify_checksum<T, O>(in bool condition, in T data, in O checksum, HashAlgorithm algo);

/***
 * Computes the checksum of the supplied data and writes it to the
 * checksum parameter.
 *
 * Calling update_checksum is only supported in the ComputeChecksum
 * control.
 *
 * @param T          Must be a tuple type where all the tuple elements
 *                   are of type bit<W>, int<W>, or varbit<W>.  The
 *                   total length of the fields must be a multiple of
 *                   the output size.
 * @param O          Output type; must be bit<X> type.
 * @param condition  If 'false' the checksum parameter is not changed
 * @param data       Data whose checksum is computed.
 * @param checksum   Checksum of the data.
 * @param algo       Algorithm to use for checksum (not all algorithms
 *                   may be supported).  Must be a compile-time
 *                   constant.
 */
@pure
extern void update_checksum<T, O>(in bool condition, in T data, inout O checksum, HashAlgorithm algo);

/***
 * verify_checksum_with_payload is identical in all ways to
 * verify_checksum, except that it includes the payload of the packet
 * in the checksum calculation.  The payload is defined as "all bytes
 * of the packet which were not parsed by the parser".
 *
 * Calling verify_checksum_with_payload is only supported in the
 * VerifyChecksum control.
 */
extern void verify_checksum_with_payload<T, O>(in bool condition, in T data, in O checksum, HashAlgorithm algo);

/**
 * update_checksum_with_payload is identical in all ways to
 * update_checksum, except that it includes the payload of the packet
 * in the checksum calculation.  The payload is defined as "all bytes
 * of the packet which were not parsed by the parser".
 *
 * Calling update_checksum_with_payload is only supported in the
 * ComputeChecksum control.
 */
@noSideEffects
extern void update_checksum_with_payload<T, O>(in bool condition, in T data, inout O checksum, HashAlgorithm algo);
// END:Checksum_extern_functions


// BEGIN:Action_multicast
/// Modify ingress output metadata to cause 0 or more copies of the
/// packet to be sent to egress processing.

/// This action does not change whether a clone or resubmit operation
/// will occur.


// BEGIN:Action_ingress_drop
/// Modify ingress output metadata to cause no packet to be sent for
/// normal egress processing.

/// This action does not change whether a clone will occur.  It will
/// prevent a packet from being resubmitted.

// BEGIN:Action_loopback_control
/// 设置网络回环标志，将数据包从网络端口回环到主机端口
@noWarn("unused")
action set_net_loopback(inout tuna_ingress_input_metadata_t meta)
{
    meta.packet_path = TUNA_PacketPath_t.FROM_NET_LOOPEDBACK;
}

/// 设置主机回环标志，将数据包从主机端口回环到网络端口
@noWarn("unused")
action set_host_loopback(inout tuna_egress_input_metadata_t meta)
{
    meta.packet_path = TUNA_PacketPath_t.FROM_HOST_LOOPEDBACK;
}
// END:Action_loopback_control

extern PacketReplicationEngine {
    PacketReplicationEngine();
    // There are no methods for this object callable from a P4
    // program.  This extern exists so it will have an instance with a
    // name that the control plane can use to make control plane API
    // calls on this object.
}

extern BufferingQueueingEngine {
    BufferingQueueingEngine();
    // There are no methods for this object callable from a P4
    // program.  See comments for PacketReplicationEngine.
}

// BEGIN:CounterType_defn
enum TUNA_CounterType_t {
    PACKETS,
    BYTES,
    PACKETS_AND_BYTES
}
// END:CounterType_defn

// BEGIN:CounterIndexMode_defn
/// Counter index mode - specifies how the counter ID is determined
enum TUNA_CounterIndexMode_t {
    DIRECT,      // Mode 0: Counter ID directly encoded in instruction
    FLOW_ID,     // Mode 1: Counter ID from metadata FLOW_ID field
    ACTION_DATA  // Mode 2: Counter ID from Action Data array
}
// END:CounterIndexMode_defn

// BEGIN:Counter_extern
/// Indirect counter is not supported
/// The counter index can be determined in three ways according to requirements:
/// - DIRECT: Index explicitly provided in count() call (constant or expression)
/// - FLOW_ID: Index obtained from metadata FLOW_ID field
/// - ACTION_DATA: Index obtained from Action Data array
///
/// @param W Counter width (typically bit<32> or bit<64>)
/// @param S Index type (typically bit<32>)

@noWarn("unused")
extern Counter<W, S> {
  /// Constructor
  /// @param n_counters Number of counter entries
  /// @param type Counter type (PACKETS, BYTES, or PACKETS_AND_BYTES)
  /// @param index_mode How the counter index is determined
  Counter(bit<32> n_counters, TUNA_CounterType_t type, TUNA_CounterIndexMode_t index_mode);

  /// Count method for DIRECT mode
  /// @param index Explicit counter index (0 to n_counters-1)
  void count(in S index);

  /// Count method for FLOW_ID mode
  /// Counter index is automatically obtained from metadata FLOW_ID
  void count();

  /// Count method for ACTION_DATA mode
  /// Counter index is obtained from Action Data array at specified offset
  /// @param ad_index Action Data array index (0-37)
  void count_from_ad(in bit<6> ad_index);
}
// END:Counter_extern


// BEGIN:Programmable_blocks
parser IngressParser<H, M, RECIRCM>(
    packet_in buffer,
    out H parsed_hdr,
    inout M user_meta,
    in tuna_ingress_parser_input_metadata_t istd,
    in RECIRCM recirculate_meta);

/*
 * The only legal statements in the body of the VerifyChecksum control
 * are: block statements, calls to the verify_checksum and
 * verify_checksum_with_payload methods, and return statements.
 */
control VerifyChecksum<H, M>(inout H hdr,
                             inout M meta);

control Ingress<H, M>(
    inout H hdr, inout M user_meta,
    in    tuna_ingress_input_metadata_t  istd,
    inout tuna_ingress_output_metadata_t ostd);

/*
 * The only legal statements in the body of the ComputeChecksum
 * control are: block statements, calls to the update_checksum and
 * update_checksum_with_payload methods, and return statements.
 */
control ComputeChecksum<H, M>(inout H hdr,
                              inout M meta);

control IngressDeparser<H, M, RECIRCM, NM>(
    packet_out buffer,
    out RECIRCM recirculate_meta,
    out NM normal_meta,
    inout H hdr,
    in M meta,
    in tuna_ingress_output_metadata_t istd);

parser EgressParser<H, M, NM>(
    packet_in buffer,
    out H parsed_hdr,
    inout M user_meta,
    in tuna_egress_parser_input_metadata_t istd,
    in NM normal_meta);

control Egress<H, M>(
    inout H hdr, inout M user_meta,
    in    tuna_egress_input_metadata_t  istd,
    inout tuna_egress_output_metadata_t ostd);

control EgressDeparser<H, M, RECIRCM>(
    packet_out buffer,
    out RECIRCM recirculate_meta,
    inout H hdr,
    in M meta,
    in tuna_egress_output_metadata_t istd);

// 修改 Pipeline 定义，同时支持 Ingress 和 Egress
package IngressPipeline<IH, IM, NM, RECIRCM>(
    IngressParser<IH, IM, RECIRCM> ip,
    VerifyChecksum<IH, IM> vr,
    Ingress<IH, IM> ig,
    ComputeChecksum<IH, IM> ck,
    IngressDeparser<IH, IM, RECIRCM, NM> id);

package EgressPipeline<EH, EM, NM, RECIRCM>(
    EgressParser<EH, EM, NM> ep,
    VerifyChecksum<EH, EM> vr,
    Egress<EH, EM> eg,
    ComputeChecksum<EH, EM> ck,
    EgressDeparser<EH, EM, RECIRCM> ed);

package TunaNic<IH, IM, EH, EM, NM, RECIRCM> (
    IngressPipeline<IH, IM, NM, RECIRCM> ingress,
    PacketReplicationEngine pre,
    EgressPipeline<EH, EM, NM, RECIRCM> egress,
    BufferingQueueingEngine bqe);

// END:Programmable_blocks

#endif   // __TUNA_P4__
