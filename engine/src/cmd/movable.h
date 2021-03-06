#ifndef MOVABLE_H
#define MOVABLE_H

#include "gfx/vec.h"
#include "vs_limits.h"
#include "gfx/quaternion.h"

struct Transformation;
class Matrix;
class Unit;
class UnitCollection;
struct Quaternion;


class Movable
{
public:
    // Fields
    //mass of this unit (may change with cargo)
    float  Mass;
    Limits limits;
    //The velocity this unit has in World Space
    Vector cumulative_velocity;
    //The force applied from outside accrued over the whole physics frame
    Vector NetForce;
    //The force applied by internal objects (thrusters)
    Vector NetLocalForce;
    //The torque applied from outside objects
    Vector NetTorque;
    //The torque applied from internal objects
    Vector NetLocalTorque;
    //the current velocities in LOCAL space (not world space)
    Vector AngularVelocity;
    Vector Velocity;

    //The previous state in last physics frame to interpolate within
    Transformation prev_physical_state;
    //The state of the current physics frame to interpolate within
    Transformation curr_physical_state;

    //Should we resolve forces on this unit (is it free to fly or in orbit)
    // TODO: this should be deleted when we separate satelites from movables
    bool resolveforces;

    //The number of frames ahead this was put in the simulation queue
    unsigned int   sim_atom_multiplier;
    //The number of frames ahead this is predicted to be scheduled in the next scheduling round
    unsigned int   predicted_priority;
    //When will physical simulation occur
    unsigned int   cur_sim_queue_slot;
    //Used with subunit scheduling, to avoid the complex ickiness of having to synchronize scattered slots
    unsigned int   last_processed_sqs;

    // TODO: this should go up to ship
    unsigned char docked;

    //The cumulative (incl subunits parents' transformation)
    Matrix cumulative_transformation_matrix;
    //The cumulative (incl subunits parents' transformation)
    Transformation cumulative_transformation;

    Vector corner_min, corner_max;
    //How big is this unit
    float radial_size;

    class graphic_options
    {
public:
        unsigned SubUnit : 1;
        unsigned RecurseIntoSubUnitsOnCollision : 1;
        unsigned missilelock : 1;
        unsigned FaceCamera : 1;
        unsigned Animating : 1;
        unsigned InWarp : 1;
        unsigned WarpRamping : 1;
        unsigned unused1 : 1;
        unsigned NoDamageParticles : 1;
        unsigned specInterdictionOnline : 1;
        unsigned char NumAnimationPoints;
        float    WarpFieldStrength;
        float    RampCounter;
        float    MinWarpMultiplier;
        float    MaxWarpMultiplier;
        graphic_options();
    }
    graphicOptions;
protected:
    //Moment of intertia of this unit
    float  Momentofinertia;
    Vector SavedAccel;
    Vector SavedAngAccel;


// Methods

public:
    Movable();

    void AddVelocity( float difficulty );
//Resolves forces of given unit on a physics frame
    virtual Vector ResolveForces( const Transformation&, const Matrix& );

    //Sets the unit-space position
    void SetPosition( const QVector &pos );

//Returns the pqr oritnattion of the unit in world space
    void SetOrientation( QVector q, QVector r );
    void SetOrientation( Quaternion Q );
    void SetOrientation( QVector p, QVector q, QVector r );
    void GetOrientation( Vector &p, Vector &q, Vector &r ) const;
    Vector GetNetAcceleration() const;
    Vector GetNetAngularAcceleration() const;
//acceleration, retrieved from NetForce - not stable (partial during simulation), use GetAcceleration()
    Vector GetAcceleration() const
    {
        return SavedAccel;
    }
    Vector GetAngularAcceleration() const
    {
        return SavedAngAccel;
    }
//acceleration, stable over the simulation
    float GetMaxAccelerationInDirectionOf( const Vector &ref, bool afterburn ) const;
//Transforms a orientation vector Up a coordinate level. Does not take position into account
    Vector UpCoordinateLevel( const Vector &v ) const;
//Transforms a orientation vector Down a coordinate level. Does not take position into account
    Vector DownCoordinateLevel( const Vector &v ) const;
//Transforms a orientation vector from world space to local space. Does not take position into account
    Vector ToLocalCoordinates( const Vector &v ) const;
//Transforms a orientation vector to world space. Does not take position into account
    Vector ToWorldCoordinates( const Vector &v ) const;

    virtual bool isPlayerShip() { return false; };

    //Updates physics given unit space transformations and if this is the last physics frame in the current gfx frame
    //Not needed here, so only in NetUnit and Unit classes
        void UpdatePhysics( const Transformation &trans,
                            const Matrix &transmat,
                            const Vector &CumulativeVelocity,
                            bool ResolveLast,
                            UnitCollection *uc,
                            Unit *superunit );
        virtual void UpdatePhysics2( const Transformation &trans,
                                     const Transformation &old_physical_state,
                                     const Vector &accel,
                                     float difficulty,
                                     const Matrix &transmat,
                                     const Vector &CumulativeVelocity,
                                     bool ResolveLast,
                                     UnitCollection *uc = NULL );

    //Returns unit-space ang velocity
    const Vector& GetAngularVelocity() const
    {
        return AngularVelocity;
    }
    //Return unit-space velocity
    const Vector& GetVelocity() const
    {
        return cumulative_velocity;
    }
    virtual Vector GetWarpVelocity() const = 0;
    virtual Vector GetWarpRefVelocity() const = 0;
    void SetVelocity( const Vector& );
    void SetAngularVelocity( const Vector& );
    float GetMoment() const
    {
        return Momentofinertia; // TODO: subclass with return Momentofinertia+fuel;
    }
    float GetMass() const
    {
        return Mass; // TODO: subclass with return Mass+fuel;
    }
    //returns the ammt of elasticity of collisions with this unit
    float GetElasticity();

    //Sets if forces should resolve on this unit or not
    void SetResolveForces( bool );

    float GetMaxWarpFieldStrength( float rampmult = 1.f ) const;
    void DecreaseWarpEnergy( bool insystem, float time = 1.0f );
    void IncreaseWarpEnergy( bool insystem, float time = 1.0f );
    //Rotates about the axis
    void Rotate( const Vector &axis );

    virtual QVector realPosition() = 0;
    virtual float CalculateNearestWarpUnit( float minmultiplier, Unit **nearest_unit, bool count_negative_warp_units ) const = 0;
    virtual void UpdatePhysics3(const Transformation &trans,
                        const Matrix &transmat,
                        bool lastframe,
                        UnitCollection *uc,
                        Unit *superunit) = 0;
};



#endif // MOVABLE_H
