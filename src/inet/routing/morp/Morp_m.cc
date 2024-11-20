//
// Generated file, do not edit! Created by opp_msgtool 6.0 from inet/routing/morp/Morp.msg.
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
#include "Morp_m.h"

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

Register_Class(MorpBeacon)

MorpBeacon::MorpBeacon() : ::inet::FieldsChunk()
{
}

MorpBeacon::MorpBeacon(const MorpBeacon& other) : ::inet::FieldsChunk(other)
{
    copy(other);
}

MorpBeacon::~MorpBeacon()
{
}

MorpBeacon& MorpBeacon::operator=(const MorpBeacon& other)
{
    if (this == &other) return *this;
    ::inet::FieldsChunk::operator=(other);
    copy(other);
    return *this;
}

void MorpBeacon::copy(const MorpBeacon& other)
{
    this->srcAddress = other.srcAddress;
    this->nextAddress = other.nextAddress;
    this->sequenceNumber = other.sequenceNumber;
    this->cost = other.cost;
    this->nextPosition = other.nextPosition;
    this->nodeDegree = other.nodeDegree;
    this->residualEnergy = other.residualEnergy;
    this->dataRate = other.dataRate;
}

void MorpBeacon::parsimPack(omnetpp::cCommBuffer *b) const
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
}

void MorpBeacon::parsimUnpack(omnetpp::cCommBuffer *b)
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
}

const Ipv4Address& MorpBeacon::getSrcAddress() const
{
    return this->srcAddress;
}

void MorpBeacon::setSrcAddress(const Ipv4Address& srcAddress)
{
    handleChange();
    this->srcAddress = srcAddress;
}

const Ipv4Address& MorpBeacon::getNextAddress() const
{
    return this->nextAddress;
}

void MorpBeacon::setNextAddress(const Ipv4Address& nextAddress)
{
    handleChange();
    this->nextAddress = nextAddress;
}

unsigned int MorpBeacon::getSequenceNumber() const
{
    return this->sequenceNumber;
}

void MorpBeacon::setSequenceNumber(unsigned int sequenceNumber)
{
    handleChange();
    this->sequenceNumber = sequenceNumber;
}

float MorpBeacon::getCost() const
{
    return this->cost;
}

void MorpBeacon::setCost(float cost)
{
    handleChange();
    this->cost = cost;
}

const Coord& MorpBeacon::getNextPosition() const
{
    return this->nextPosition;
}

void MorpBeacon::setNextPosition(const Coord& nextPosition)
{
    handleChange();
    this->nextPosition = nextPosition;
}

int MorpBeacon::getNodeDegree() const
{
    return this->nodeDegree;
}

void MorpBeacon::setNodeDegree(int nodeDegree)
{
    handleChange();
    this->nodeDegree = nodeDegree;
}

double MorpBeacon::getResidualEnergy() const
{
    return this->residualEnergy;
}

void MorpBeacon::setResidualEnergy(double residualEnergy)
{
    handleChange();
    this->residualEnergy = residualEnergy;
}

double MorpBeacon::getDataRate() const
{
    return this->dataRate;
}

void MorpBeacon::setDataRate(double dataRate)
{
    handleChange();
    this->dataRate = dataRate;
}

class MorpBeaconDescriptor : public omnetpp::cClassDescriptor
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
    };
  public:
    MorpBeaconDescriptor();
    virtual ~MorpBeaconDescriptor();

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

Register_ClassDescriptor(MorpBeaconDescriptor)

MorpBeaconDescriptor::MorpBeaconDescriptor() : omnetpp::cClassDescriptor(omnetpp::opp_typename(typeid(inet::MorpBeacon)), "inet::FieldsChunk")
{
    propertyNames = nullptr;
}

MorpBeaconDescriptor::~MorpBeaconDescriptor()
{
    delete[] propertyNames;
}

bool MorpBeaconDescriptor::doesSupport(omnetpp::cObject *obj) const
{
    return dynamic_cast<MorpBeacon *>(obj)!=nullptr;
}

const char **MorpBeaconDescriptor::getPropertyNames() const
{
    if (!propertyNames) {
        static const char *names[] = {  nullptr };
        omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
        const char **baseNames = base ? base->getPropertyNames() : nullptr;
        propertyNames = mergeLists(baseNames, names);
    }
    return propertyNames;
}

const char *MorpBeaconDescriptor::getProperty(const char *propertyName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? base->getProperty(propertyName) : nullptr;
}

int MorpBeaconDescriptor::getFieldCount() const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? 8+base->getFieldCount() : 8;
}

unsigned int MorpBeaconDescriptor::getFieldTypeFlags(int field) const
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
    };
    return (field >= 0 && field < 8) ? fieldTypeFlags[field] : 0;
}

const char *MorpBeaconDescriptor::getFieldName(int field) const
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
    };
    return (field >= 0 && field < 8) ? fieldNames[field] : nullptr;
}

int MorpBeaconDescriptor::findField(const char *fieldName) const
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
    return base ? base->findField(fieldName) : -1;
}

const char *MorpBeaconDescriptor::getFieldTypeString(int field) const
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
    };
    return (field >= 0 && field < 8) ? fieldTypeStrings[field] : nullptr;
}

const char **MorpBeaconDescriptor::getFieldPropertyNames(int field) const
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

const char *MorpBeaconDescriptor::getFieldProperty(int field, const char *propertyName) const
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

int MorpBeaconDescriptor::getFieldArraySize(omnetpp::any_ptr object, int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldArraySize(object, field);
        field -= base->getFieldCount();
    }
    MorpBeacon *pp = omnetpp::fromAnyPtr<MorpBeacon>(object); (void)pp;
    switch (field) {
        default: return 0;
    }
}

void MorpBeaconDescriptor::setFieldArraySize(omnetpp::any_ptr object, int field, int size) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldArraySize(object, field, size);
            return;
        }
        field -= base->getFieldCount();
    }
    MorpBeacon *pp = omnetpp::fromAnyPtr<MorpBeacon>(object); (void)pp;
    switch (field) {
        default: throw omnetpp::cRuntimeError("Cannot set array size of field %d of class 'MorpBeacon'", field);
    }
}

const char *MorpBeaconDescriptor::getFieldDynamicTypeString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldDynamicTypeString(object,field,i);
        field -= base->getFieldCount();
    }
    MorpBeacon *pp = omnetpp::fromAnyPtr<MorpBeacon>(object); (void)pp;
    switch (field) {
        default: return nullptr;
    }
}

std::string MorpBeaconDescriptor::getFieldValueAsString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValueAsString(object,field,i);
        field -= base->getFieldCount();
    }
    MorpBeacon *pp = omnetpp::fromAnyPtr<MorpBeacon>(object); (void)pp;
    switch (field) {
        case FIELD_srcAddress: return pp->getSrcAddress().str();
        case FIELD_nextAddress: return pp->getNextAddress().str();
        case FIELD_sequenceNumber: return ulong2string(pp->getSequenceNumber());
        case FIELD_cost: return double2string(pp->getCost());
        case FIELD_nextPosition: return pp->getNextPosition().str();
        case FIELD_nodeDegree: return long2string(pp->getNodeDegree());
        case FIELD_residualEnergy: return double2string(pp->getResidualEnergy());
        case FIELD_dataRate: return double2string(pp->getDataRate());
        default: return "";
    }
}

void MorpBeaconDescriptor::setFieldValueAsString(omnetpp::any_ptr object, int field, int i, const char *value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValueAsString(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    MorpBeacon *pp = omnetpp::fromAnyPtr<MorpBeacon>(object); (void)pp;
    switch (field) {
        case FIELD_sequenceNumber: pp->setSequenceNumber(string2ulong(value)); break;
        case FIELD_cost: pp->setCost(string2double(value)); break;
        case FIELD_nodeDegree: pp->setNodeDegree(string2long(value)); break;
        case FIELD_residualEnergy: pp->setResidualEnergy(string2double(value)); break;
        case FIELD_dataRate: pp->setDataRate(string2double(value)); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'MorpBeacon'", field);
    }
}

omnetpp::cValue MorpBeaconDescriptor::getFieldValue(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValue(object,field,i);
        field -= base->getFieldCount();
    }
    MorpBeacon *pp = omnetpp::fromAnyPtr<MorpBeacon>(object); (void)pp;
    switch (field) {
        case FIELD_srcAddress: return omnetpp::toAnyPtr(&pp->getSrcAddress()); break;
        case FIELD_nextAddress: return omnetpp::toAnyPtr(&pp->getNextAddress()); break;
        case FIELD_sequenceNumber: return (omnetpp::intval_t)(pp->getSequenceNumber());
        case FIELD_cost: return (double)(pp->getCost());
        case FIELD_nextPosition: return omnetpp::toAnyPtr(&pp->getNextPosition()); break;
        case FIELD_nodeDegree: return pp->getNodeDegree();
        case FIELD_residualEnergy: return pp->getResidualEnergy();
        case FIELD_dataRate: return pp->getDataRate();
        default: throw omnetpp::cRuntimeError("Cannot return field %d of class 'MorpBeacon' as cValue -- field index out of range?", field);
    }
}

void MorpBeaconDescriptor::setFieldValue(omnetpp::any_ptr object, int field, int i, const omnetpp::cValue& value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValue(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    MorpBeacon *pp = omnetpp::fromAnyPtr<MorpBeacon>(object); (void)pp;
    switch (field) {
        case FIELD_sequenceNumber: pp->setSequenceNumber(omnetpp::checked_int_cast<unsigned int>(value.intValue())); break;
        case FIELD_cost: pp->setCost(static_cast<float>(value.doubleValue())); break;
        case FIELD_nodeDegree: pp->setNodeDegree(omnetpp::checked_int_cast<int>(value.intValue())); break;
        case FIELD_residualEnergy: pp->setResidualEnergy(value.doubleValue()); break;
        case FIELD_dataRate: pp->setDataRate(value.doubleValue()); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'MorpBeacon'", field);
    }
}

const char *MorpBeaconDescriptor::getFieldStructName(int field) const
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

omnetpp::any_ptr MorpBeaconDescriptor::getFieldStructValuePointer(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldStructValuePointer(object, field, i);
        field -= base->getFieldCount();
    }
    MorpBeacon *pp = omnetpp::fromAnyPtr<MorpBeacon>(object); (void)pp;
    switch (field) {
        case FIELD_srcAddress: return omnetpp::toAnyPtr(&pp->getSrcAddress()); break;
        case FIELD_nextAddress: return omnetpp::toAnyPtr(&pp->getNextAddress()); break;
        case FIELD_nextPosition: return omnetpp::toAnyPtr(&pp->getNextPosition()); break;
        default: return omnetpp::any_ptr(nullptr);
    }
}

void MorpBeaconDescriptor::setFieldStructValuePointer(omnetpp::any_ptr object, int field, int i, omnetpp::any_ptr ptr) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldStructValuePointer(object, field, i, ptr);
            return;
        }
        field -= base->getFieldCount();
    }
    MorpBeacon *pp = omnetpp::fromAnyPtr<MorpBeacon>(object); (void)pp;
    switch (field) {
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'MorpBeacon'", field);
    }
}

}  // namespace inet

namespace omnetpp {

}  // namespace omnetpp

