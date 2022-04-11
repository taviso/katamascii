#ifndef __COMPAT_H
#define __COMPAT_H

#if CP_VERSION_MAJOR < 7
static inline cpVect cpBBCenter(cpBB bb)
{
    return cpvlerp(cpv(bb.l, bb.b), cpv(bb.r, bb.t), 0.5f);
}
#endif

#endif
