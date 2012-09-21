t = db.pushTrim;
t.drop();

t.save({ _id: 1, a: [1, 2] });
t.update({ _id: 1 }, {$push: { a: { $each: [3], $trim: 3 } } } );
assert.eq( "1,2,3" , t.findOne().a.toString() , "A" );
t.update({ _id: 1 }, {$push: { a: { $each: [4], $trim: 3 } } } );
assert.eq( "2,3,4" , t.findOne().a.toString() , "A" );

