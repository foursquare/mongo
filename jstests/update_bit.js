var t = db.bit_update_test;

t.drop();

// Basic upsert validations:

// Nothing is created by bit mod if upsert not set
t.update({_id: 1}, {$bit: {flags: {and: 1}}});
t.update({_id: 2}, {$bit: {flags: {or: 1}}});
assert.eq(0, t.find().toArray().length);

// New documents are created by bit mod if upsert is set
// If bit would be unmodified from zero, no change is made
// TODO: should this equal zero?
t.update({_id: 1}, {$bit: {flags: {and: 1}}}, true);
t.update({_id: 2}, {$bit: {flags: {or: 1}}}, true);
assert.isnull(t.find({_id:1}).flags);
assert.eq(1, t.findOne({_id:2}).flags);

// Basic update validation:

// If field does not exist, and no change is made, field is not created
t.insert({_id:3});
t.insert({_id:4});
t.update({_id:3}, {$bit: {flags: {and: 1}}});
t.update({_id:4}, {$bit: {flags: {or: 1}}});
assert.isnull(t.findOne({_id:3}).flags);
assert.eq(1, t.findOne({_id:4}).flags);

// If zero field does exist, and no change is made, field remains zero
t.insert({_id:5, flags:NumberLong(0)});
t.insert({_id:6, flags:NumberLong(0)});
t.update({_id:5}, {$bit: {flags: {and: 1}}});
t.update({_id:6}, {$bit: {flags: {or: 1}}});
assert.eq(0, t.findOne({_id:5}).flags);
assert.eq(1, t.findOne({_id:6}).flags);

// If non-zero field does exist, and no change is made, field unchanged
t.insert({_id:7, flags:1});
t.insert({_id:8, flags:1});
t.update({_id:7}, {$bit: {flags: {and: 1}}});
t.update({_id:8}, {$bit: {flags: {or: 1}}});
assert.eq(1, t.findOne({_id:7}).flags);
assert.eq(1, t.findOne({_id:8}).flags);

// And and or modifications on update
t.insert({_id:9, flags:NumberLong(1)});
t.insert({_id:10, flags:NumberLong(1)});
t.update({_id:9}, {$bit: {flags: {and: 2}}});
t.update({_id:10}, {$bit: {flags: {or: 2}}});
assert.eq(0, t.findOne({_id:9}).flags);
assert.eq(3, t.findOne({_id:10}).flags);

// Multi-field upsert:

// Upserting to multiple fields simultaneously should behave for each field as for one field
t.update({_id:11}, {$bit: {flags: {and: 1}, flags2: {and: 1}}}, true);
t.update({_id:12}, {$bit: {flags: {or: 1}, flags2: {or: 1}}}, true);
t.update({_id:13}, {$bit: {flags: {and: 1}, flags2: {or: 1}}}, true);
assert.isnull(t.findOne({_id:11}).flags);
assert.isnull(t.findOne({_id:11}).flags2);
assert.eq(1, t.findOne({_id:12}).flags);
assert.eq(1, t.findOne({_id:12}).flags2);
assert.isnull(t.findOne({_id:13}).flags);
assert.eq(1, t.findOne({_id:13}).flags2);

// Multi-mod upsert:
// Multiple updates with same path will be de-duped
// Multiple paths will be applied in order
t.update({_id:14}, {$bit: {flags: {and: 1, and: 2}}}, true);
t.update({_id:15}, {$bit: {flags: {or: 1, or: 2}}}, true);
t.update({_id:16}, {$bit: {flags: {and: 1, or: 2}}}, true);
t.update({_id:17}, {$bit: {flags: {or: 1, and: 2}}}, true);
assert.isnull(t.findOne({_id:14}).flags);
assert.eq(2, t.findOne({_id:15}).flags);
assert.eq(2, t.findOne({_id:16}).flags);
assert.isnull(t.findOne({_id:17}).flags);

// Multi-field update:

// Updating multiple fields simultaneously should behave for each field as for one field
t.insert({_id:18, flags: NumberLong(0), flags2: NumberLong(0)});
t.insert({_id:19, flags: NumberLong(0), flags2: NumberLong(0)});
t.insert({_id:20, flags: NumberLong(0), flags2: NumberLong(0)});
t.update({_id:18}, {$bit: {flags: {and: 1}, flags2: {and: 1}}}, true);
t.update({_id:19}, {$bit: {flags: {or: 1}, flags2: {or: 1}}}, true);
t.update({_id:20}, {$bit: {flags: {and: 1}, flags2: {or: 1}}}, true);
assert.eq(0, t.findOne({_id:18}).flags);
assert.eq(0, t.findOne({_id:18}).flags2);
assert.eq(1, t.findOne({_id:19}).flags);
assert.eq(1, t.findOne({_id:19}).flags2);
assert.eq(0, t.findOne({_id:20}).flags);
assert.eq(1, t.findOne({_id:20}).flags2);

// Multi-mod update:
// Mulitple updates with same path will be de-duped
// Multiple paths will be applied in order
t.insert({_id:21, flags: NumberLong(0)});
t.insert({_id:22, flags: NumberLong(0)});
t.insert({_id:23, flags: NumberLong(0)});
t.insert({_id:24, flags: NumberLong(0)});
t.update({_id:21}, {$bit: {flags: {and: 1, and: 2}}});
t.update({_id:22}, {$bit: {flags: {or: 1, or: 2}}});
t.update({_id:23}, {$bit: {flags: {and: 1, or: 2}}});
t.update({_id:24}, {$bit: {flags: {or: 1, and: 2}}});
assert.eq(0, t.findOne({_id:21}).flags);
assert.eq(2, t.findOne({_id:22}).flags);
assert.eq(2, t.findOne({_id:23}).flags);
assert.eq(0, t.findOne({_id:24}).flags);
