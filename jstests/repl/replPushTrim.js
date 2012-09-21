rt = new ReplTest( "replPushTrim" );

master = rt.start( true );
mc = master.getDB( 'd' )[ 'c' ];

targetId = 1;
mc.insert( { } );
master.getDB( 'd' ).getLastError();

slave = rt.start( false );
sc = slave.getDB( 'd' )[ 'c' ];

// Wait for slave to start cloning.
assert.soon( function() { c = sc.count(); /*print( c );*/ return c > 0; } );

// Push an upsert
mc.update( { _id:targetId }, { $push:{ val: { $each: [1,2,3], $trim: 3 } } }, true );

mc.insert( { _id:'sentinel' } );

// Wait for the updates to be applied.
assert.soon( function() { return sc.count( { _id:'sentinel' } ) > 0; } );

// Check that the val array is as expected.
assert.eq( [ 1, 2, 3 ], mc.findOne( { _id:targetId } ).val );
assert.eq( [ 1, 2, 3 ], sc.findOne( { _id:targetId } ).val );

// Push an update
mc.update( { _id:targetId }, { $push:{ val: { $each: [4,5], $trim: 3 } } });

mc.insert( { _id:'sentinel2' } );

// Wait for the updates to be applied.
assert.soon( function() { return sc.count( { _id:'sentinel2' } ) > 0; } );

// Check that the val array is as expected.
assert.eq( [ 3, 4, 5 ], mc.findOne( { _id:targetId } ).val );
assert.eq( [ 3, 4, 5 ], sc.findOne( { _id:targetId } ).val );

// Push an update with only $each, no trim
mc.update( { _id:targetId }, { $push:{ val: { $each: [6,7] } } });

mc.insert( { _id:'sentinel3' } );

// Wait for the updates to be applied.
assert.soon( function() { return sc.count( { _id:'sentinel3' } ) > 0; } );

// Check that the val array is as expected.
assert.eq( [ 3, 4, 5, 6, 7 ], mc.findOne( { _id:targetId } ).val );
assert.eq( [ 3, 4, 5, 6, 7 ], sc.findOne( { _id:targetId } ).val );


