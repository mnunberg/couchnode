#ifndef CAS_H_
#define CAS_H_

namespace Couchnode
{
class Cas
{
public:
    static bool GetCas(v8::Handle<v8::Value>, uint64_t*);
    static v8::Handle<v8::Value> CreateCas(uint64_t);
};

} // namespace Couchnode

#endif /* CAS_H_ */
