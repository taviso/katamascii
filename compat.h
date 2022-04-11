#ifndef __COMPAT_H
#define __COMPAT_H

#if CP_VERSION_MAJOR < 7
static inline cpVect cpBBCenter(cpBB bb)
{
    return cpvlerp(cpv(bb.l, bb.b), cpv(bb.r, bb.t), 0.5f);
}
#endif

#if CP_VERSION_MAJOR > 6

#define cpBodySetPos cpBodySetPosition
#define cpBoxShapeNew2(body, box) cpBoxShapeNew2((body), (box), 0.0)
#define cpMomentForSegment(m, a, b) cpMomentForSegment((m), (a), (b), 0.0)
#define cpBodyApplyImpulse cpBodyApplyImpulseAtLocalPoint

#define cpSpaceSetDefaultCollisionHandler(s, b, pre, post, sep, u) do { \
    cpCollisionHandler *c = cpSpaceAddDefaultCollisionHandler(s);       \
    c->beginFunc = b;                                                   \
} while (false)

#endif


#endif
