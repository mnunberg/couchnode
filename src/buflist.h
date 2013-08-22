#ifndef COUCHNODE_BUFLIST_H
#define COUCHNODE_BUFLIST_H
#include <cstdlib>
namespace Couchnode {

class BufferList
{
public:
    /**
     * This class helps avoid a lot of small allocations; we might want to
     * optimize this a bit later of course :)
     *
     * For now, it serves as a convenient place to allocate all our string
     * pointers without each command worrying about freeing them.
     */
    BufferList() : curBuf(NULL), bytesUsed(0), bytesAllocated(defaultSize) { }

    char *getBuffer(size_t len) {
        char *ret;

        if (len >= bytesAllocated) {
            std::cerr << "Requested string too big.. \n";
            ret = new char[len];
            return ret;
        }

        if (!curBuf) {
            if (!(curBuf = new char[bytesAllocated])) {
                return NULL;
            }

            bufList.push_back(curBuf);
        }

        if (bytesAvailable() > len) {
            ret = curBuf;
            bytesUsed += len;
            curBuf += len;

        } else {
            curBuf = NULL;
            bytesUsed = 0;
            return getBuffer(len);
        }

        return ret;
    }

    BufferList(BufferList& other)
    {
        this->bufList= other.bufList;
        bytesUsed = other.bytesUsed;
        bytesAllocated = other.bytesAllocated;
        curBuf = other.curBuf;

        other.bufList.clear();
    }

    ~BufferList() {
        for (unsigned int ii = 0; ii < bufList.size(); ii++) {
            delete[] bufList[ii];
        }
    }

private:
    inline size_t bytesAvailable() {
        return bytesAllocated - bytesUsed;
    }

    static const unsigned int defaultSize = 1024;
    std::vector<char *> bufList;
    char *curBuf;
    size_t bytesUsed;
    size_t bytesAllocated;

};
};
#endif
