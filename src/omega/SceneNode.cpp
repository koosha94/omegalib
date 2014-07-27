/******************************************************************************
 * THE OMEGA LIB PROJECT
 *-----------------------------------------------------------------------------
 * Copyright 2010-2013		Electronic Visualization Laboratory, 
 *							University of Illinois at Chicago
 * Authors:										
 *  Alessandro Febretti		febret@gmail.com
 *-----------------------------------------------------------------------------
 * Copyright (c) 2010-2013, Electronic Visualization Laboratory,  
 * University of Illinois at Chicago
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 * 
 * Redistributions of source code must retain the above copyright notice, this 
 * list of conditions and the following disclaimer. Redistributions in binary 
 * form must reproduce the above copyright notice, this list of conditions and 
 * the following disclaimer in the documentation and/or other materials provided 
 * with the distribution. 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE  GOODS OR 
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, 
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *-----------------------------------------------------------------------------
 * What's in this file
 *	The core class for the omegalib scene tree.
 ******************************************************************************/
#include "omega/DrawInterface.h"
#include "omega/SceneNode.h"
#include "omega/Renderable.h"
#include "omega/Engine.h"
#include "omega/Camera.h"
#include "omega/ModuleServices.h"
#include "omega/glheaders.h"
#include "omega/TrackedObject.h"

using namespace omega;


///////////////////////////////////////////////////////////////////////////////
SceneNode* SceneNode::create(const String& name)
{
    SceneNode* sn = new SceneNode(Engine::instance(), name);
    Engine::instance()->getScene()->addChild(sn);
    return sn;
}

///////////////////////////////////////////////////////////////////////////////
void SceneNode::addListener(SceneNodeListener* listener)
{
    myListeners.push_back(listener);
}

///////////////////////////////////////////////////////////////////////////////
void SceneNode::removeListener(SceneNodeListener* listener)
{
    myListeners.remove(listener);
}

///////////////////////////////////////////////////////////////////////////////
bool SceneNode::isVisible()
{ 
    return myVisible; 
}

///////////////////////////////////////////////////////////////////////////////
void SceneNode::setVisible(bool value)
{ 
    if(myListeners.size() != 0)
    {
        foreach(SceneNodeListener* l, myListeners)
        {
            l->onVisibleChanged(this, value);
        }
    }
    myVisible = value; 
}

///////////////////////////////////////////////////////////////////////////////
void SceneNode::setChildrenVisible(bool value)
{
    foreach(Node* c, mChildrenList)
    {
        SceneNode* snc = dynamic_cast<SceneNode*>(c);
        if(snc != NULL) 
        {
            snc->setVisible(value);
            snc->setChildrenVisible(value);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
void SceneNode::setParent(Node* parent)
{
    // Save the current scene attachment state
    bool wasAttached = isAttachedToScene();

    // If changed parent is a scene node, call listeners.
    // NOTE: We call listeners only for SceneNode parents since in the future 
    // Node & ScneNode classes should be unified, and this simplifies the API.
    SceneNode* snparent = dynamic_cast<SceneNode*>(parent);
    if(snparent != NULL || parent == NULL)
    {
        if(myListeners.size() != 0)
        {
            foreach(SceneNodeListener* l, myListeners)
            {
                l->onParentChanged(this, snparent);
            }
        }
    }
    Node::setParent(parent);

    // If the attachment state changed, notify listeners.
    bool isAttached = isAttachedToScene();
    if(wasAttached != isAttached)
    {
        if(isAttached) onAttachedToScene();
        else onDetachedFromScene();
    }
}

///////////////////////////////////////////////////////////////////////////////
void SceneNode::onAttachedToScene()
{
    foreach(SceneNodeListener* l, myListeners)
    {
        l->onAttachedToScene(this);
    }
    // Broadcast to children
    foreach(Node* c, mChildrenList)
    {
        SceneNode* snc = dynamic_cast<SceneNode*>(c);
        if(snc != NULL) snc->onAttachedToScene();
    }
}

///////////////////////////////////////////////////////////////////////////////
void SceneNode::onDetachedFromScene()
{
    foreach(SceneNodeListener* l, myListeners)
    {
        l->onDetachedFromScene(this);
    }
    // Broadcast to children
    foreach(Node* c, mChildrenList)
    {
        SceneNode* snc = dynamic_cast<SceneNode*>(c);
        if(snc != NULL) snc->onDetachedFromScene();
    }
}

///////////////////////////////////////////////////////////////////////////////
void SceneNode::setSelected(bool value)
{
    mySelected = value;
    if(myListeners.size() != 0)
    {
        foreach(SceneNodeListener* l, myListeners)
        {
            l->onSelectedChanged(this, value);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
bool SceneNode::isSelected()
{
    return mySelected;
}

///////////////////////////////////////////////////////////////////////////////
bool SceneNode::isAttachedToScene()
{
    Node* cur = this;
    while(cur != NULL)
    {
        if(cur == getEngine()->getScene()) return true; 
        cur = cur->getParent();
    }
    return false;
}

///////////////////////////////////////////////////////////////////////////////
void SceneNode::addComponent(NodeComponent* o) 
{ 
    myObjects.push_back(o); 
    o->attach(this);
    //needUpdate();
    // Force a bounding box update.
    updateBoundingBox(true);
    // If the object has not been initialized yet, do it now.
    if(!o->isInitialized()) o->initialize(myServer);
}

///////////////////////////////////////////////////////////////////////////////
int SceneNode::getNumComponents()
{ 
    return myObjects.size(); 
}

///////////////////////////////////////////////////////////////////////////////
void SceneNode::removeComponent(NodeComponent* o) 
{
    myObjects.remove(o);
    o->detach(this);
    updateBoundingBox();
    //needUpdate();
}

///////////////////////////////////////////////////////////////////////////////
void SceneNode::draw(const DrawContext& context)
{
    if(myVisible)
    {
        //if(myChanged) updateTransform();

        if(myBoundingBoxVisible) drawBoundingBox();

        // Draw drawables attached to this node.
        foreach(NodeComponent* d, myObjects)
        {
            d->draw(context);
        }

        // Draw children nodes.
        foreach(Node* child, getChildren())
        {
            SceneNode* n = dynamic_cast<SceneNode*>(child);
            n->draw(context);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
void SceneNode::update(bool updateChildren, bool parentHasChanged)
{
    // Short circuit the off case
    if (!updateChildren && !mNeedParentUpdate && !mNeedChildUpdate && !parentHasChanged)
    {
        return;
    }

    Node::update(updateChildren, parentHasChanged);
}

///////////////////////////////////////////////////////////////////////////////
void SceneNode::update(const UpdateContext& context)
{
    // Step 1: traverse the scene graph and invoke update on all nodes.
    // Nodes will have the chance to modify their own transform in this phase.
    // For instance, nodes that have to face cameras update transforms in this
    // step. User-defined updateTraversal methods for custom SceneNode classes
    // may perform other operations in this step.
    updateTraversal(context);

    // Step 2: update all needed transforms in the node hierarchy
    update(true, false);

    // Step 3: update all node components. In this step, all nodes have 
    // up-to-date transforms, so we can consistently update all attached node
    // components
    updateComponents(context);
}

///////////////////////////////////////////////////////////////////////////////
void SceneNode::updateTraversal(const UpdateContext& context)
{
    // Reorient the node if a facing camera is set
    if(myFacingCamera != NULL)
    {
        Vector3f up = Vector3f::UnitY();
        // If fixed Y is disabled, re-orient the up vector based on the
        // camera orientation
        if(!myFacingCameraFixedY) up = myFacingCamera->getOrientation() * up;
        Vector3f pos = myFacingCamera->getPosition() + myFacingCamera->getHeadOffset();
        lookAt(pos, up);
    }

    // Update children
    foreach(Node* child, getChildren())
    {
        SceneNode* n = dynamic_cast<SceneNode*>(child);
        if(n) n->updateTraversal(context);
    }
}

///////////////////////////////////////////////////////////////////////////////
void SceneNode::updateComponents(const UpdateContext& context)
{
    // Update attached components
    foreach(NodeComponent* d, myObjects)
    {
        d->update(context);
    }
    // Update components of children nodes
    foreach(Node* child, getChildren())
    {
        SceneNode* n = dynamic_cast<SceneNode*>(child);
        if(n) n->updateComponents(context);
    }
}

///////////////////////////////////////////////////////////////////////////////
void SceneNode::needUpdate(bool forceParentUpdate)
{
    Node::needUpdate();
    requestBoundingBoxUpdate();
}

///////////////////////////////////////////////////////////////////////////////
const AlignedBox3& SceneNode::getBoundingBox() 
{ 
    updateBoundingBox();
    return myBBox; 
}

///////////////////////////////////////////////////////////////////////////////
const Sphere& SceneNode::getBoundingSphere() 
{ 
    updateBoundingBox();
    return myBSphere; 
}

///////////////////////////////////////////////////////////////////////////////
const Vector3f& SceneNode::getBoundMinimum()
{
    updateBoundingBox();
    if(!myBBox.isFinite())
    {
        ofwarn("SceneNode::getBoundMinimum: non-finite bounds for scene node %1%", %getName());
        return Vector3f::Zero();
    }
    return myBBox.getMinimum();
}

///////////////////////////////////////////////////////////////////////////////
const Vector3f& SceneNode::getBoundMaximum()
{
    updateBoundingBox();
    if(!myBBox.isFinite())
    {
        ofwarn("SceneNode::getBoundMaximum: non-finite bounds for scene node %1%", %getName());
        return Vector3f::Zero();
    }
    return myBBox.getMaximum();
}

///////////////////////////////////////////////////////////////////////////////
const Vector3f SceneNode::getBoundCenter()
{
    updateBoundingBox();
    if(!myBBox.isFinite())
    {
        ofwarn("SceneNode::getBoundCenter: non-finite bounds for scene node %1%", %getName());
        return Vector3f::Zero();
    }
    return myBBox.getCenter();
}

///////////////////////////////////////////////////////////////////////////////
float SceneNode::getBoundRadius()
{
    updateBoundingBox();
    if(!myBBox.isFinite())
    {
        ofwarn("SceneNode::getBoundRadius: non-finite bounds for scene node %1%", %getName());
        return 0.0f;
    }
    return myBSphere.getRadius();
}

///////////////////////////////////////////////////////////////////////////////
void SceneNode::updateBoundingBox(bool force)
{
    // Exit now if bounding box does not need an update.
    if(!force && !needsBoundingBoxUpdate() && !mCachedTransformOutOfDate) return;

    // Reset bounding box.
    myBBox.setNull();

    foreach(NodeComponent* d, myObjects)
    {
        if(d->hasBoundingBox())
        {
            if(d->needsBoundingBoxUpdate()) d->updateBoundingBox();
            const AlignedBox3& bbox = *(d->getBoundingBox());
            myBBox.merge(bbox);
        }
    }

    myBBox.transformAffine(getFullTransform());

    foreach(Node* child, getChildren())
    {
        SceneNode* n = dynamic_cast<SceneNode*>(child);
        if(n != NULL)
        {
            const AlignedBox3& bbox = n->getBoundingBox();
            myBBox.merge(bbox);
        }
    }

    if(!myBBox.isNull())
    {
        // Compute bounding sphere.
        myBSphere = Sphere(myBBox.getCenter(), myBBox.getHalfSize().maxCoeff());
    }

    //SceneNode* parent = dynamic_cast<SceneNode*>(getParent());
    //if(parent != NULL)
    //{
    //	parent->updateBoundingBox(true);
    //}

    myNeedsBoundingBoxUpdate = false;
}

///////////////////////////////////////////////////////////////////////////////
void SceneNode::drawBoundingBox()
{
    updateBoundingBox();
    glPushAttrib(GL_ENABLE_BIT);
    glDisable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);
    //glDepthFunc(GL_GREATER);
    
    glColor3fv(myBoundingBoxColor.data());
    glBegin(GL_LINES);

    glVertex3f(myBBox[0][0], myBBox[0][1], myBBox[0][2]);
    glVertex3f(myBBox[1][0], myBBox[0][1], myBBox[0][2]);

    glVertex3f(myBBox[0][0], myBBox[0][1], myBBox[0][2]);
    glVertex3f(myBBox[0][0], myBBox[0][1], myBBox[1][2]);

    glVertex3f(myBBox[0][0], myBBox[0][1], myBBox[0][2]);
    glVertex3f(myBBox[0][0], myBBox[1][1], myBBox[0][2]);

    glVertex3f(myBBox[1][0], myBBox[1][1], myBBox[1][2]);
    glVertex3f(myBBox[1][0], myBBox[0][1], myBBox[1][2]);

    glVertex3f(myBBox[1][0], myBBox[1][1], myBBox[1][2]);
    glVertex3f(myBBox[1][0], myBBox[1][1], myBBox[0][2]);

    glVertex3f(myBBox[1][0], myBBox[1][1], myBBox[1][2]);
    glVertex3f(myBBox[0][0], myBBox[1][1], myBBox[1][2]);

    glVertex3f(myBBox[1][0], myBBox[0][1], myBBox[1][2]);
    glVertex3f(myBBox[0][0], myBBox[0][1], myBBox[1][2]);

    glVertex3f(myBBox[1][0], myBBox[0][1], myBBox[1][2]);
    glVertex3f(myBBox[1][0], myBBox[0][1], myBBox[0][2]);

    glVertex3f(myBBox[1][0], myBBox[1][1], myBBox[0][2]);
    glVertex3f(myBBox[0][0], myBBox[1][1], myBBox[0][2]);

    glVertex3f(myBBox[1][0], myBBox[1][1], myBBox[0][2]);
    glVertex3f(myBBox[1][0], myBBox[0][1], myBBox[0][2]);

    glVertex3f(myBBox[0][0], myBBox[1][1], myBBox[1][2]);
    glVertex3f(myBBox[0][0], myBBox[0][1], myBBox[1][2]);

    glVertex3f(myBBox[0][0], myBBox[1][1], myBBox[1][2]);
    glVertex3f(myBBox[0][0], myBBox[1][1], myBBox[0][2]);
    glEnd();

    glPopAttrib();
}

///////////////////////////////////////////////////////////////////////////////
bool SceneNode::hit(const Ray& ray, Vector3f* hitPoint, HitType hitType)
{
    if(hitType == HitBest)
    {
        bool hasCustomIntersectors = false;
        foreach(NodeComponent* iso, myObjects)
        {
            if(iso->hasCustomRayIntersector())
            {
                hasCustomIntersectors = true;
                if(iso->intersectRay(ray, hitPoint)) return true;
            }
        }

        // If no attached scene object has a custom ray intersector, fall back to the bounding sphere technique
        if(!hasCustomIntersectors) hitType = HitBoundingSphere;
    }

    if(hitType == HitBoundingSphere)
    {
        const Sphere& s = getBoundingSphere();
        std::pair<bool, omega::real> h = ray.intersects(s);
        if(h.first)
        {
            (*hitPoint) = ray.getPoint(h.second);
        }
        return h.first;
    }
    return false;
}

///////////////////////////////////////////////////////////////////////////////
void SceneNode::followTrackable(int trackableId)
{
    if(myTracker == NULL)
    {
        myTracker = new TrackedObject();
        ModuleServices::addModule(myTracker);
        myTracker->setSceneNode(this);
    }
    myTracker->setTrackableSourceId(trackableId);
}

///////////////////////////////////////////////////////////////////////////////
void SceneNode::setFollowOffset(const Vector3f& offset, const Quaternion& ooffset)
{
    myTracker->setOffset(offset);
    myTracker->setOrientationOffset(ooffset);
}

///////////////////////////////////////////////////////////////////////////////
void SceneNode::unfollow()
{
    if(myTracker != NULL)
    {
        ModuleServices::removeModule(myTracker);
        myTracker = NULL;
    }
}
