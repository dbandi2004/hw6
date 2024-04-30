#ifndef HT_H
#define HT_H

#include <vector>
#include <iostream>
#include <cmath>
#include <stdexcept>

typedef size_t HASH_INDEX_T;

// Base Prober class
template <typename KeyType>
struct Prober {
    HASH_INDEX_T start_;    // initial hash location
    HASH_INDEX_T m_;        // table size
    size_t numProbes_;      // probe attempts for statistic tracking
    static const HASH_INDEX_T npos = (HASH_INDEX_T)-1; // used to indicate probing failed
    
    virtual void init(HASH_INDEX_T start, HASH_INDEX_T m, const KeyType& key) {
        start_ = start;
        m_ = m;
        numProbes_ = 0;
    }
    
    virtual HASH_INDEX_T next() {
        throw std::logic_error("Prober not implemented...should use derived class");
    }
};

// Linear Prober
template <typename KeyType>
struct LinearProber : public Prober<KeyType> {
    HASH_INDEX_T next() override {
        if(this->numProbes_ >= this->m_) {
            return this->npos;
        }
        HASH_INDEX_T loc = (this->start_ + this->numProbes_) % this->m_;
        this->numProbes_++;
        return loc;
    }
};

// Double Hash Prober
template <typename KeyType, typename Hash2>
struct DoubleHashProber : public Prober<KeyType> {
    Hash2 h2_; // Secondary hash function
    HASH_INDEX_T dhstep_; // Step size for double hashing
    static const HASH_INDEX_T DOUBLE_HASH_MOD_VALUES[]; 
    static const int DOUBLE_HASH_MOD_SIZE;

    DoubleHashProber(const Hash2& h2 = Hash2()) : h2_(h2) {}

    void init(HASH_INDEX_T start, HASH_INDEX_T m, const KeyType& key) override {
        Prober<KeyType>::init(start, m, key);
        HASH_INDEX_T modulus = findModulusToUseFromTableSize(m);
        dhstep_ = modulus - h2_(key) % modulus;
    }

    HASH_INDEX_T next() override {
        if(this->numProbes_ >= this->m_) {
            return this->npos;
        }
        HASH_INDEX_T loc = (this->start_ + this->numProbes_ * dhstep_) % this->m_;
        this->numProbes_++;
        return loc;
    }

private:
    HASH_INDEX_T findModulusToUseFromTableSize(HASH_INDEX_T currTableSize) {
        for (int i = 0; i < DOUBLE_HASH_MOD_SIZE; ++i) {
            if (DOUBLE_HASH_MOD_VALUES[i] >= currTableSize) {
                return DOUBLE_HASH_MOD_VALUES[i - 1];
            }
        }
        return DOUBLE_HASH_MOD_VALUES[DOUBLE_HASH_MOD_SIZE - 1];
    }
};

template <typename KeyType, typename Hash2>
const HASH_INDEX_T DoubleHashProber<KeyType, Hash2>::DOUBLE_HASH_MOD_VALUES[] = {
    7, 19, 43, 89, 193, 389, 787, 1583, 3191, 6397, 12841, 25703, 51431, 102871,
    205721, 411503, 823051, 1646221, 3292463, 6584957, 13169963, 26339921, 52679927,
    105359939, 210719881, 421439749, 842879563, 1685759113
};

template <typename KeyType, typename Hash2>
const int DoubleHashProber<KeyType, Hash2>::DOUBLE_HASH_MOD_SIZE = sizeof(DoubleHashProber<KeyType, Hash2>::DOUBLE_HASH_MOD_VALUES) / sizeof(HASH_INDEX_T);

template<
    typename K, 
    typename V, 
    typename Prober = LinearProber<K>,
    typename Hash = std::hash<K>, 
    typename KEqual = std::equal_to<K> >
class HashTable {
public:
    typedef K KeyType;
    typedef V ValueType;
    typedef std::pair<KeyType, ValueType> ItemType;
    typedef Hash Hasher;
    struct HashItem {
        ItemType item;
        bool deleted;
        HashItem(const ItemType& newItem) : item(newItem), deleted(false) {}
    };

    HashTable(double resizeAlpha = 0.4, const Prober& prober = Prober(), const Hasher& hash = Hasher(), const KEqual& kequal = KEqual()) : hash_(hash), kequal_(kequal), prober_(prober), totalProbes_(0), mIndex_(0) {
        table_.resize(CAPACITIES[mIndex_], nullptr);
    }

    ~HashTable() {
        for (auto item : table_) {
            delete item;
        }
    }

    bool empty() const {
        for (auto item : table_) {
            if (item && !item->deleted) {
                return false;
            }
        }
        return true;
    }

    size_t size() const {
        size_t count = 0;
        for (auto item : table_) {
            if (item && !item->deleted) {
                count++;
            }
        }
        return count;
    }

    void insert(const ItemType& p) {
        // Resize if load factor threshold exceeded
        if (size() / static_cast<double>(CAPACITIES[mIndex_]) >= 0.4) {
            resize();
        }

        HASH_INDEX_T index = probe(p.first);
        if (index == npos) {
            throw std::logic_error("No free location found");
        }
        
        if (table_[index] == nullptr) {
            table_[index] = new HashItem(p);
        } else {
            table_[index]->item.second = p.second;
            table_[index]->deleted = false;
        }
    }

    void remove(const KeyType& key) {
        HASH_INDEX_T index = probe(key);
        if (index != npos && table_[index] != nullptr) {
            table_[index]->deleted = true;
        }
    }

    ItemType const* find(const KeyType& key) const {
        HASH_INDEX_T index = probe(key);
        if (index == npos || table_[index] == nullptr || table_[index]->deleted) {
            return nullptr;
        }
        return &table_[index]->item;
    }

    ItemType* find(const KeyType& key) {
        return const_cast<ItemType*>(static_cast<const HashTable*>(this)->find(key));
    }

    const ValueType& at(const KeyType& key) const {
        const ItemType* item = find(key);
        if (!item) {
            throw std::out_of_range("Key not found");
        }
        return item->second;
    }

    ValueType& at(const KeyType& key) {
        return const_cast<ValueType&>(static_cast<const HashTable*>(this)->at(key));
    }

    const ValueType& operator[](const KeyType& key) const {
        return at(key);
    }

    ValueType& operator[](const KeyType& key) {
        return at(key);
    }

    void reportAll(std::ostream& out) const {
        for (size_t i = 0; i < CAPACITIES[mIndex_]; ++i) {
            if (table_[i] && !table_[i]->deleted) {
                out << "Bucket " << i << ": " << table_[i]->item.first << " - " << table_[i]->item.second << std::endl;
            }
        }
    }

    void resize() {
        size_t newIdx = mIndex_ + 1;
        if (newIdx >= sizeof(CAPACITIES) / sizeof(HASH_INDEX_T)) {
            throw std::logic_error("Maximum capacity reached, cannot resize further");
        }

        std::vector<HashItem*> newTable(CAPACITIES[newIdx], nullptr);
        for (auto& item : table_) {
            if (item && !item->deleted) {
                HASH_INDEX_T index = probe(item->item.first);
                newTable[index] = item;
            } else {
                delete item;
            }
        }

        table_ = std::move(newTable);
        mIndex_ = newIdx;
    }

private:
    HASH_INDEX_T probe(const KeyType& key) const {
        HASH_INDEX_T h = hash_(key) % CAPACITIES[mIndex_];
        prober_.init(h, CAPACITIES[mIndex_], key);
        HASH_INDEX_T loc = prober_.next();
        while (loc != Prober::npos) {
            if (table_[loc] == nullptr || table_[loc]->deleted || kequal_(table_[loc]->item.first, key)) {
                return loc;
            }
            loc = prober_.next();
        }
        return Prober::npos;
    }

    static const HASH_INDEX_T CAPACITIES[];
    std::vector<HashItem*> table_;
    Hasher hash_;
    KEqual kequal_;
    mutable Prober prober_;
    mutable size_t totalProbes_;
    size_t mIndex_;
};

template<typename K, typename V, typename Prober, typename Hash, typename KEqual>
const HASH_INDEX_T HashTable<K,V,Prober,Hash,KEqual>::CAPACITIES[] = {
    11, 23, 47, 97, 197, 397, 797, 1597, 3203, 6421, 12853, 25717, 51437, 102877,
    205759, 411527, 823117, 1646237, 3292489, 6584983, 13169977, 26339969, 52679969,
    105359969, 210719881, 421439783, 842879579, 1685759167
};

#endif
