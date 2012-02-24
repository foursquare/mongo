
t = db.pushMax
t.drop();

t.save( { _id : 1, a : [ 1, 2 ] } );
t.update( { _id : 1 } , { $pushMax : { a : {max: 3, data: 3 } } } );
assert.eq( "1,2,3" , t.findOne().a.toString() , "A" );
t.update( { _id : 1 } , { $pushMax : { a : {max: 3, data: 4 } } } );
assert.eq( "2,3,4" , t.findOne().a.toString() , "B" );