//
// Generated file, do not edit! Created by opp_msgtool 6.0 from inet/routing/mlmorp/Mlmorp.msg.
//

// Disable warnings about unused variables, empty switch stmts, etc:
#ifdef _MSC_VER
#  pragma warning(disable:4101)
#  pragma warning(disable:4065)
#endif

#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wshadow"
#  pragma clang diagnostic ignored "-Wconversion"
#  pragma clang diagnostic ignored "-Wunused-parameter"
#  pragma clang diagnostic ignored "-Wc++98-compat"
#  pragma clang diagnostic ignored "-Wunreachable-code-break"
#  pragma clang diagnostic ignored "-Wold-style-cast"
#elif defined(__GNUC__)
#  pragma GCC diagnostic ignored "-Wshadow"
#  pragma GCC diagnostic ignored "-Wconversion"
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#  pragma GCC diagnostic ignored "-Wold-style-cast"
#  pragma GCC diagnostic ignored "-Wsuggest-attribute=noreturn"
#  pragma GCC diagnostic ignored "-Wfloat-conversion"
#endif

#include <iostream>
#include <sstream>
#include <memory>
#include <type_traits>
#include "Mlmorp_m.h"

namespace omnetpp {

// Template pack/unpack rules. They are declared *after* a1l type-specific pack functions for multiple reasons.
// They are in the omnetpp namespace, to allow them to be found by argument-dependent lookup via the cCommBuffer argument

// Packing/unpacking an std::vector
template<typename T, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::vector<T,A>& v)
{
    int n = v.size();
    doParsimPacking(buffer, n);
    for (int i = 0; i < n; i++)
        doParsimPacking(buffer, v[i]);
}

template<typename T, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::vector<T,A>& v)
{
    int n;
    doParsimUnpacking(buffer, n);
    v.resize(n);
    for (int i = 0; i < n; i++)
        doParsimUnpacking(buffer, v[i]);
}

// Packing/unpacking an std::list
template<typename T, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::list<T,A>& l)
{
    doParsimPacking(buffer, (int)l.size());
    for (typename std::list<T,A>::const_iterator it = l.begin(); it != l.end(); ++it)
        doParsimPacking(buffer, (T&)*it);
}

template<typename T, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::list<T,A>& l)
{
    int n;
    doParsimUnpacking(buffer, n);
    for (int i = 0; i < n; i++) {
        l.push_back(T());
        doParsimUnpacking(buffer, l.back());
    }
}

// Packing/unpacking an std::set
template<typename T, typename Tr, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::set<T,Tr,A>& s)
{
    doParsimPacking(buffer, (int)s.size());
    for (typename std::set<T,Tr,A>::const_iterator it = s.begin(); it != s.end(); ++it)
        doParsimPacking(buffer, *it);
}

template<typename T, typename Tr, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::set<T,Tr,A>& s)
{
    int n;
    doParsimUnpacking(buffer, n);
    for (int i = 0; i < n; i++) {
        T x;
        doParsimUnpacking(buffer, x);
        s.insert(x);
    }
}

// Packing/unpacking an std::map
template<typename K, typename V, typename Tr, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::map<K,V,Tr,A>& m)
{
    doParsimPacking(buffer, (int)m.size());
    for (typename std::map<K,V,Tr,A>::const_iterator it = m.begin(); it != m.end(); ++it) {
        doParsimPacking(buffer, it->first);
        doParsimPacking(buffer, it->second);
    }
}

template<typename K, typename V, typename Tr, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::map<K,V,Tr,A>& m)
{
    int n;
    doParsimUnpacking(buffer, n);
    for (int i = 0; i < n; i++) {
        K k; V v;
        doParsimUnpacking(buffer, k);
        doParsimUnpacking(buffer, v);
        m[k] = v;
    }
}

// Default pack/unpack function for arrays
template<typename T>
void doParsimArrayPacking(omnetpp::cCommBuffer *b, const T *t, int n)
{
    for (int i = 0; i < n; i++)
        doParsimPacking(b, t[i]);
}

template<typename T>
void doParsimArrayUnpacking(omnetpp::cCommBuffer *b, T *t, int n)
{
    for (int i = 0; i < n; i++)
        doParsimUnpacking(b, t[i]);
}

// Default rule to prevent compiler from choosing base class' doParsimPacking() function
template<typename T>
void doParsimPacking(omnetpp::cCommBuffer *, const T& t)
{
    throw omnetpp::cRuntimeError("Parsim error: No doParsimPacking() function for type %s", omnetpp::opp_typename(typeid(t)));
}

template<typename T>
void doParsimUnpacking(omnetpp::cCommBuffer *, T& t)
{
    throw omnetpp::cRuntimeError("Parsim error: No doParsimUnpacking() function for type %s", omnetpp::opp_typename(typeid(t)));
}

}  // namespace omnetpp

namespace inet {

Register_Class(MlmorpBeacon)

MlmorpBeacon::MlmorpBeacon() : ::inet::FieldsChunk()
{
}

MlmorpBeacon::MlmorpBeacon(const MlmorpBeacon& other) : ::inet::FieldsChunk(other)
{
    copy(other);
}

MlmorpBeacon::~MlmorpBeacon()
{
}

MlmorpBeacon& MlmorpBeacon::operator=(const MlmorpBeacon& other)
{
    if (this == &other) return *this;
    ::inet::FieldsChunk::operator=(other);
    copy(other);
    return *this;
}

void MlmorpBeacon::copy(const MlmorpBeacon& other)
{
    this->srcAddress = other.srcAddress;
    this->nextAddress = other.nextAddress;
    this->sequenceNumber = other.sequenceNumber;
    this->cost = other.cost;
    this->nextPosition = other.nextPosition;
    this->nodeDegree = other.nodeDegree;
    this->residualEnergy = other.residualEnergy;
    this->dataRate = other.dataRate;
    this->signalPower = other.signalPower;
    this->snir = other.snir;
    this->packetDelay = other.packetDelay;
}

void MlmorpBeacon::parsimPack(omnetpp::cCommBuffer *b) const
{
    ::inet::FieldsChunk::parsimPack(b);
    doParsimPacking(b,this->srcAddress);
    doParsimPacking(b,this->nextAddress);
    doParsimPacking(b,this->sequenceNumber);
    doParsimPacking(b,this->cost);
    doParsimPacking(b,this->nextPosition);
    doParsimPacking(b,this->nodeDegree);
    doParsimPacking(b,this->residualEnergy);
    doParsimPacking(b,this->dataRate);
    doParsimPacking(b,this->signalPower);
    doParsimPacking(b,this->snir);
    doParsimPacking(b,this->packetDelay);
}

void MlmorpBeacon::parsimUnpack(omnetpp::cCommBuffer *b)
{
    ::inet::FieldsChunk::parsimUnpack(b);
    doParsimUnpacking(b,this->srcAddress);
    doParsimUnpacking(b,this->nextAddress);
    doParsimUnpacking(b,this->sequenceNumber);
    doParsimUnpacking(b,this->cost);
    doParsimUnpacking(b,this->nextPosition);
    doParsimUnpacking(b,this->nodeDegree);
    doParsimUnpacking(b,this->residualEnergy);
    doParsimUnpacking(b,this->dataRate);
    doParsimUnpacking(b,this->signalPower);
    doParsimUnpacking(b,this->snir);
    doParsimUnpacking(b,this->packetDelay);
}

const Ipv4Address& MlmorpBeacon::getSrcAddress() const
{
    return this->srcAddress;
}

void MlmorpBeacon::setSrcAddress(const Ipv4Address& srcAddress)
{
    handleChange();
    this->srcAddress = srcAddress;
}

const Ipv4Address& MlmorpBeacon::getNextAddress() const
{
    return this->nextAddress;
}

void MlmorpBeacon::setNextAddress(const Ipv4Address& nextAddress)
{
    handleChange();
    this->nextAddress = nextAddress;
}

unsigned int MlmorpBeacon::getSequenceNumber() const
{
    return this->sequenceNumber;
}

void MlmorpBeacon::setSequenceNumber(unsigned int sequenceNumber)
{
    handleChange();
    this->sequenceNumber = sequenceNumber;
}

float MlmorpBeacon::getCost() const
{
    return this->cost;
}

void MlmorpBeacon::setCost(float cost)
{
    handleChange();
    this->cost = cost;
}

const Coord& MlmorpBeacon::getNextPosition() const
{
    return this->nextPosition;
}

void MlmorpBeacon::setNextPosition(const Coord& nextPosition)
{
    handleChange();
    this->nextPosition = nextPosition;
}

int MlmorpBeacon::getNodeDegree() const
{
    return this->nodeDegree;
}

void MlmorpBeacon::setNodeDegree(int nodeDegree)
{
    handleChange();
    this->nodeDegree = nodeDegree;
}

double MlmorpBeacon::getResidualEnergy() const
{
    return this->residualEnergy;
}

void MlmorpBeacon::setResidualEnergy(double residualEnergy)
{
    handleChange();
    this->residualEnergy = residualEnergy;
}

double MlmorpBeacon::getDataRate() const
{
    return this->dataRate;
}

void MlmorpBeacon::setDataRate(double dataRate)
{
    handleChange();
    this->dataRate = dataRate;
}

double MlmorpBeacon::getSignalPower() const
{
    return this->signalPower;
}

void MlmorpBeacon::setSignalPower(double signalPower)
{
    handleChange();
    this->signalPower = signalPower;
}

double MlmorpBeacon::getSnir() const
{
    return this->snir;
}

void MlmorpBeacon::setSnir(double snir)
{
    handleChange();
    this->snir = snir;
}

::omnetpp::simtime_t MlmorpBeacon::getPacketDelay() const
{
    return this->packetDelay;
}

void MlmorpBeacon::setPacketDelay(::omnetpp::simtime_t packetDelay)
{
    handleChange();
    this->packetDelay = packetDelay;
}

class MlmorpBeaconDescriptor : public omnetpp::cClassDescriptor
{
  private:
    mutable const char **propertyNames;
    enum FieldConstants {
        FIELD_srcAddress,
        FIELD_nextAddress,
        FIELD_sequenceNumber,
        FIELD_cost,
        FIELD_nextPosition,
        FIELD_nodeDegree,
        FIELD_residualEnergy,
        FIELD_dataRate,
        FIELD_signalPower,
        FIELD_snir,
        FIELD_packetDelay,
    };
  public:
    MlmorpBeaconDescriptor();
    virtual ~MlmorpBeaconDescriptor();

    virtual bool doesSupport(omnetpp::cObject *obj) const override;
    virtual const char **getPropertyNames() const override;
    virtual const char *getProperty(const char *propertyName) const override;
    virtual int getFieldCount() const override;
    virtual const char *getFieldName(int field) const override;
    virtual int findField(const char *fieldName) const override;
    virtual unsigned int getFieldTypeFlags(int field) const override;
    virtual const char *getFieldTypeString(int field) const override;
    virtual const char **getFieldPropertyNames(int field) const override;
    virtual const char *getFieldProperty(int field, const char *propertyName) const override;
    virtual int getFieldArraySize(omnetpp::any_ptr object, int field) const override;
    virtual void setFieldArraySize(omnetpp::any_ptr object, int field, int size) const override;

    virtual const char *getFieldDynamicTypeString(omnetpp::any_ptr object, int field, int i) const override;
    virtual std::string getFieldValueAsString(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldValueAsString(omnetpp::any_ptr object, int field, int i, const char *value) const override;
    virtual omnetpp::cValue getFieldValue(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldValue(omnetpp::any_ptr object, int field, int i, const omnetpp::cValue& value) const override;

    virtual const char *getFieldStructName(int field) const override;
    virtual omnetpp::any_ptr getFieldStructValuePointer(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldStructValuePointer(omnetpp::any_ptr object, int field, int i, omnetpp::any_ptr ptr) const override;
};

Register_ClassDescriptor(MlmorpBeaconDescriptor)

MlmorpBeaconDescriptor::MlmorpBeaconDescriptor() : omnetpp::cClassDescriptor(omnetpp::opp_typename(typeid(inet::MlmorpBeacon)), "inet::FieldsChunk")
{
    propertyNames = nullptr;
}

MlmorpBeaconDescriptor::~MlmorpBeaconDescriptor()
{
    delete[] propertyNames;
}

bool MlmorpBeaconDescriptor::doesSupport(omnetpp::cObject *obj) const
{
    return dynamic_cast<MlmorpBeacon *>(obj)!=nullptr;
}

const char **MlmorpBeaconDescriptor::getPropertyNames() const
{
    if (!propertyNames) {
        static const char *names[] = {  nullptr };
        omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
        const char **baseNames = base ? base->getPropertyNames() : nullptr;
        propertyNames = mergeLists(baseNames, names);
    }
    return propertyNames;
}

const char *MlmorpBeaconDescriptor::getProperty(const char *propertyName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? base->getProperty(propertyName) : nullptr;
}

int MlmorpBeaconDescriptor::getFieldCount() const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? 11+base->getFieldCount() : 11;
}

unsigned int MlmorpBeaconDescriptor::getFieldTypeFlags(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeFlags(field);
        field -= base->getFieldCount();
    }
    static unsigned int fieldTypeFlags[] = {
        0,    // FIELD_srcAddress
        0,    // FIELD_nextAddress
        FD_ISEDITABLE,    // FIELD_sequenceNumber
        FD_ISEDITABLE,    // FIELD_cost
        FD_ISCOMPOUND,    // FIELD_nextPosition
        FD_ISEDITABLE,    // FIELD_nodeDegree
        FD_ISEDITABLE,    // FIELD_residualEnergy
        FD_ISEDITABLE,    // FIELD_dataRate
        FD_ISEDITABLE,    // FIELD_signalPower
        FD_ISEDITABLE,    // FIELD_snir
        FD_ISEDITABLE,    // FIELD_packetDelay
    };
    return (field >= 0 && field < 11) ? fieldTypeFlags[field] : 0;
}

const char *MlmorpBeaconDescriptor::getFieldName(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldName(field);
        field -= base->getFieldCount();
    }
    static const char *fieldNames[] = {
        "srcAddress",
        "nextAddress",
        "sequenceNumber",
        "cost",
        "nextPosition",
        "nodeDegree",
        "residualEnergy",
        "dataRate",
        "signalPower",
        "snir",
        "packetDelay",
    };
    return (field >= 0 && field < 11) ? fieldNames[field] : nullptr;
}

int MlmorpBeaconDescriptor::findField(const char *fieldName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    int baseIndex = base ? base->getFieldCount() : 0;
    if (strcmp(fieldName, "srcAddress") == 0) return baseIndex + 0;
    if (strcmp(fieldName, "nextAddress") == 0) return baseIndex + 1;
    if (strcmp(fieldName, "sequenceNumber") == 0) return baseIndex + 2;
    if (strcmp(fieldName, "cost") == 0) return baseIndex + 3;
    if (strcmp(fieldName, "nextPosition") == 0) return baseIndex + 4;
    if (strcmp(fieldName, "nodeDegree") == 0) return baseIndex + 5;
    if (strcmp(fieldName, "residualEnergy") == 0) return baseIndex + 6;
    if (strcmp(fieldName, "dataRate") == 0) return baseIndex + 7;
    if (strcmp(fieldName, "signalPower") == 0) return baseIndex + 8;
    if (strcmp(fieldName, "snir") == 0) return baseIndex + 9;
    if (strcmp(fieldName, "packetDelay") == 0) return baseIndex + 10;
    return base ? base->findField(fieldName) : -1;
}

const char *MlmorpBeaconDescriptor::getFieldTypeString(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeString(field);
        field -= base->getFieldCount();
    }
    static const char *fieldTypeStrings[] = {
        "inet::Ipv4Address",    // FIELD_srcAddress
        "inet::Ipv4Address",    // FIELD_nextAddress
        "unsigned int",    // FIELD_sequenceNumber
        "float",    // FIELD_cost
        "inet::Coord",    // FIELD_nextPosition
        "int",    // FIELD_nodeDegree
        "double",    // FIELD_residualEnergy
        "double",    // FIELD_dataRate
        "double",    // FIELD_signalPower
        "double",    // FIELD_snir
        "omnetpp::simtime_t",    // FIELD_packetDelay
    };
    return (field >= 0 && field < 11) ? fieldTypeStrings[field] : nullptr;
}

const char **MlmorpBeaconDescriptor::getFieldPropertyNames(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldPropertyNames(field);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

const char *MlmorpBeaconDescriptor::getFieldProperty(int field, const char *propertyName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldProperty(field, propertyName);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

int MlmorpBeaconDescriptor::getFieldArraySize(omnetpp::any_ptr object, int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldArraySize(object, field);
        field -= base->getFieldCount();
    }
    MlmorpBeacon *pp = omnetpp::fromAnyPtr<MlmorpBeacon>(object); (void)pp;
    switch (field) {
        default: return 0;
    }
}

void MlmorpBeaconDescriptor::setFieldArraySize(omnetpp::any_ptr object, int field, int size) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldArraySize(object, field, size);
            return;
        }
        field -= base->getFieldCount();
    }
    MlmorpBeacon *pp = omnetpp::fromAnyPtr<MlmorpBeacon>(object); (void)pp;
    switch (field) {
        default: throw omnetpp::cRuntimeError("Cannot set array size of field %d of class 'MlmorpBeacon'", field);
    }
}

const char *MlmorpBeaconDescriptor::getFieldDynamicTypeString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldDynamicTypeString(object,field,i);
        field -= base->getFieldCount();
    }
    MlmorpBeacon *pp = omnetpp::fromAnyPtr<MlmorpBeacon>(object); (void)pp;
    switch (field) {
        default: return nullptr;
    }
}

std::string MlmorpBeaconDescriptor::getFieldValueAsString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValueAsString(object,field,i);
        field -= base->getFieldCount();
    }
    MlmorpBeacon *pp = omnetpp::fromAnyPtr<MlmorpBeacon>(object); (void)pp;
    switch (field) {
        case FIELD_srcAddress: return pp->getSrcAddress().str();
        case FIELD_nextAddress: return pp->getNextAddress().str();
        case FIELD_sequenceNumber: return ulong2string(pp->getSequenceNumber());
        case FIELD_cost: return double2string(pp->getCost());
        case FIELD_nextPosition: return pp->getNextPosition().str();
        case FIELD_nodeDegree: return long2string(pp->getNodeDegree());
        case FIELD_residualEnergy: return double2string(pp->getResidualEnergy());
        case FIELD_dataRate: return double2string(pp->getDataRate());
        case FIELD_signalPower: return double2string(pp->getSignalPower());
        case FIELD_snir: return double2string(pp->getSnir());
        case FIELD_packetDelay: return simtime2string(pp->getPacketDelay());
        default: return "";
    }
}

void MlmorpBeaconDescriptor::setFieldValueAsString(omnetpp::any_ptr object, int field, int i, const char *value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValueAsString(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    MlmorpBeacon *pp = omnetpp::fromAnyPtr<MlmorpBeacon>(object); (void)pp;
    switch (field) {
        case FIELD_sequenceNumber: pp->setSequenceNumber(string2ulong(value)); break;
        case FIELD_cost: pp->setCost(string2double(value)); break;
        case FIELD_nodeDegree: pp->setNodeDegree(string2long(value)); break;
        case FIELD_residualEnergy: pp->setResidualEnergy(string2double(value)); break;
        case FIELD_dataRate: pp->setDataRate(string2double(value)); break;
        case FIELD_signalPower: pp->setSignalPower(string2double(value)); break;
        case FIELD_snir: pp->setSnir(string2double(value)); break;
        case FIELD_packetDelay: pp->setPacketDelay(string2simtime(value)); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'MlmorpBeacon'", field);
    }
}

omnetpp::cValue MlmorpBeaconDescriptor::getFieldValue(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValue(object,field,i);
        field -= base->getFieldCount();
    }
    MlmorpBeacon *pp = omnetpp::fromAnyPtr<MlmorpBeacon>(object); (void)pp;
    switch (field) {
        case FIELD_srcAddress: return omnetpp::toAnyPtr(&pp->getSrcAddress()); break;
        case FIELD_nextAddress: return omnetpp::toAnyPtr(&pp->getNextAddress()); break;
        case FIELD_sequenceNumber: return (omnetpp::intval_t)(pp->getSequenceNumber());
        case FIELD_cost: return (double)(pp->getCost());
        case FIELD_nextPosition: return omnetpp::toAnyPtr(&pp->getNextPosition()); break;
        case FIELD_nodeDegree: return pp->getNodeDegree();
        case FIELD_residualEnergy: return pp->getResidualEnergy();
        case FIELD_dataRate: return pp->getDataRate();
        case FIELD_signalPower: return pp->getSignalPower();
        case FIELD_snir: return pp->getSnir();
        case FIELD_packetDelay: return pp->getPacketDelay().dbl();
        default: throw omnetpp::cRuntimeError("Cannot return field %d of class 'MlmorpBeacon' as cValue -- field index out of range?", field);
    }
}

void MlmorpBeaconDescriptor::setFieldValue(omnetpp::any_ptr object, int field, int i, const omnetpp::cValue& value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValue(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    MlmorpBeacon *pp = omnetpp::fromAnyPtr<MlmorpBeacon>(object); (void)pp;
    switch (field) {
        case FIELD_sequenceNumber: pp->setSequenceNumber(omnetpp::checked_int_cast<unsigned int>(value.intValue())); break;
        case FIELD_cost: pp->setCost(static_cast<float>(value.doubleValue())); break;
        case FIELD_nodeDegree: pp->setNodeDegree(omnetpp::checked_int_cast<int>(value.intValue())); break;
        case FIELD_residualEnergy: pp->setResidualEnergy(value.doubleValue()); break;
        case FIELD_dataRate: pp->setDataRate(value.doubleValue()); break;
        case FIELD_signalPower: pp->setSignalPower(value.doubleValue()); break;
        case FIELD_snir: pp->setSnir(value.doubleValue()); break;
        case FIELD_packetDelay: pp->setPacketDelay(value.doubleValue()); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'MlmorpBeacon'", field);
    }
}

const char *MlmorpBeaconDescriptor::getFieldStructName(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldStructName(field);
        field -= base->getFieldCount();
    }
    switch (field) {
        case FIELD_nextPosition: return omnetpp::opp_typename(typeid(Coord));
        default: return nullptr;
    };
}

omnetpp::any_ptr MlmorpBeaconDescriptor::getFieldStructValuePointer(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldStructValuePointer(object, field, i);
        field -= base->getFieldCount();
    }
    MlmorpBeacon *pp = omnetpp::fromAnyPtr<MlmorpBeacon>(object); (void)pp;
    switch (field) {
        case FIELD_srcAddress: return omnetpp::toAnyPtr(&pp->getSrcAddress()); break;
        case FIELD_nextAddress: return omnetpp::toAnyPtr(&pp->getNextAddress()); break;
        case FIELD_nextPosition: return omnetpp::toAnyPtr(&pp->getNextPosition()); break;
        default: return omnetpp::any_ptr(nullptr);
    }
}

void MlmorpBeaconDescriptor::setFieldStructValuePointer(omnetpp::any_ptr object, int field, int i, omnetpp::any_ptr ptr) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldStructValuePointer(object, field, i, ptr);
            return;
        }
        field -= base->getFieldCount();
    }
    MlmorpBeacon *pp = omnetpp::fromAnyPtr<MlmorpBeacon>(object); (void)pp;
    switch (field) {
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'MlmorpBeacon'", field);
    }
}

}  // namespace inet

namespace omnetpp {

}  // namespace omnetpp

