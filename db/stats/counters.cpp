// counters.cpp
/*
 *    Copyright (C) 2010 10gen Inc.
 *
 *    This program is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "pch.h"
#include "../jsobj.h"
#include "counters.h"

namespace mongo {

    OpCounters::OpCounters() {
        int zero = 0;

        BSONObjBuilder b;
        b.append( "insert" , zero );
        b.append( "query" , zero );
        b.append( "update" , zero );
        b.append( "delete" , zero );
        b.append( "getmore" , zero );
        b.append( "command" , zero );
        _obj = b.obj();

        _insert = (AtomicUInt*)_obj["insert"].value();
        _query = (AtomicUInt*)_obj["query"].value();
        _update = (AtomicUInt*)_obj["update"].value();
        _delete = (AtomicUInt*)_obj["delete"].value();
        _getmore = (AtomicUInt*)_obj["getmore"].value();
        _command = (AtomicUInt*)_obj["command"].value();
    }

    void OpCounters::gotOp( int op , bool isCommand ) {
        switch ( op ) {
        case dbInsert: /*gotInsert();*/ break; // need to handle multi-insert
        case dbQuery:
            if ( isCommand )
                gotCommand();
            else
                gotQuery();
            break;

        case dbUpdate: gotUpdate(); break;
        case dbDelete: gotDelete(); break;
        case dbGetMore: gotGetMore(); break;
        case dbKillCursors:
        case opReply:
        case dbMsg:
            break;
        default: log() << "OpCounters::gotOp unknown op: " << op << endl;
        }
    }

    BSONObj& OpCounters::getObj() {
        const unsigned MAX = 1 << 30;
        RARELY {
            bool wrap =
            _insert->get() > MAX ||
            _query->get() > MAX ||
            _update->get() > MAX ||
            _delete->get() > MAX ||
            _getmore->get() > MAX ||
            _command->get() > MAX;

            if ( wrap ) {
                _insert->zero();
                _query->zero();
                _update->zero();
                _delete->zero();
                _getmore->zero();
                _command->zero();
            }

        }
        return _obj;
    }
    
    NsOpCounters::NsOpCounters() {
      
    }

    AtomicUInt * NsOpCounters::getInsert(const StringData& ns) {
      return counterFor(ns)->getInsert();
    }
    AtomicUInt * NsOpCounters::getQuery(const StringData& ns) {
      return counterFor(ns)->getQuery();
    }
    AtomicUInt * NsOpCounters::getUpdate(const StringData& ns) {
      return counterFor(ns)->getUpdate();
    }
    AtomicUInt * NsOpCounters::getDelete(const StringData& ns) {
      return counterFor(ns)->getDelete();
    }

    void NsOpCounters::incInsertInWriteLock(const StringData& ns, int n) {
      counterFor(ns)->incInsertInWriteLock(n);
    }
    void NsOpCounters::gotInsert(const StringData& ns) {
      counterFor(ns)->gotInsert();
    }
    void NsOpCounters::gotQuery(const StringData& ns) {
      counterFor(ns)->gotQuery();
    }
    void NsOpCounters::gotUpdate(const StringData& ns) {
      counterFor(ns)->gotUpdate();
    }
    void NsOpCounters::gotDelete(const StringData& ns) {
      counterFor(ns)->gotDelete();
    }


    void NsOpCounters::gotOp( const StringData& ns , int op , bool isCommand ) {
      counterFor(ns)->gotOp( op , isCommand );
    }

    void NsOpCounters::getObj( BSONObjBuilder& b ) {
      std::map<std::string, shared_ptr<OpCounters> >::iterator it;
      BSONArrayBuilder hosts( b.subarrayStart( "opcountersNS" ) );
      for ( it = _nsCounterMap.begin() ; it != _nsCounterMap.end(); it++ ) {
        hosts.append( BSON (
          "ns" << (*it).first <<
          "opcounters" << (*it).second.get()->getObj()
        ) );
      }
      hosts.done();
    }

    IndexCounters::IndexCounters() {
        _memSupported = _pi.blockCheckSupported();

        _btreeMemHits = 0;
        _btreeMemMisses = 0;
        _btreeAccesses = 0;


        _maxAllowed = ( numeric_limits< long long >::max() ) / 2;
        _resets = 0;

        _sampling = 0;
        _samplingrate = 100;
    }

    void IndexCounters::append( BSONObjBuilder& b ) {
        if ( ! _memSupported ) {
            b.append( "note" , "not supported on this platform" );
            return;
        }

        BSONObjBuilder bb( b.subobjStart( "btree" ) );
        bb.appendNumber( "accesses" , _btreeAccesses );
        bb.appendNumber( "hits" , _btreeMemHits );
        bb.appendNumber( "misses" , _btreeMemMisses );

        bb.append( "resets" , _resets );

        bb.append( "missRatio" , (_btreeAccesses ? (_btreeMemMisses / (double)_btreeAccesses) : 0) );

        bb.done();

        if ( _btreeAccesses > _maxAllowed ) {
            _btreeAccesses = 0;
            _btreeMemMisses = 0;
            _btreeMemHits = 0;
            _resets++;
        }
    }

    FlushCounters::FlushCounters()
        : _total_time(0)
        , _flushes(0)
        , _last()
    {}

    void FlushCounters::flushed(int ms) {
        _flushes++;
        _total_time += ms;
        _last_time = ms;
        _last = jsTime();
    }

    void FlushCounters::append( BSONObjBuilder& b ) {
        b.appendNumber( "flushes" , _flushes );
        b.appendNumber( "total_ms" , _total_time );
        b.appendNumber( "average_ms" , (_flushes ? (_total_time / double(_flushes)) : 0.0) );
        b.appendNumber( "last_ms" , _last_time );
        b.append("last_finished", _last);
    }


    void GenericCounter::hit( const string& name , int count ) {
        scoped_lock lk( _mutex );
        _counts[name]++;
    }

    BSONObj GenericCounter::getObj() {
        BSONObjBuilder b(128);
        {
            mongo::mutex::scoped_lock lk( _mutex );
            for ( map<string,long long>::iterator i=_counts.begin(); i!=_counts.end(); i++ ) {
                b.appendNumber( i->first , i->second );
            }
        }
        return b.obj();
    }


    void NetworkCounter::hit( long long bytesIn , long long bytesOut ) {
        const long long MAX = 1ULL << 60;

        // don't care about the race as its just a counter
        bool overflow = _bytesIn > MAX || _bytesOut > MAX;

        if ( overflow ) {
            _lock.lock();
            _overflows++;
            _bytesIn = bytesIn;
            _bytesOut = bytesOut;
            _requests = 1;
            _lock.unlock();
        }
        else {
            _lock.lock();
            _bytesIn += bytesIn;
            _bytesOut += bytesOut;
            _requests++;
            _lock.unlock();
        }
    }

    void NetworkCounter::append( BSONObjBuilder& b ) {
        _lock.lock();
        b.appendNumber( "bytesIn" , _bytesIn );
        b.appendNumber( "bytesOut" , _bytesOut );
        b.appendNumber( "numRequests" , _requests );
        _lock.unlock();
    }


    OpCounters globalOpCounters;
    OpCounters replOpCounters;
    NsOpCounters nsOpCounters;
    IndexCounters globalIndexCounters;
    FlushCounters globalFlushCounters;
    NetworkCounter networkCounter;

}
