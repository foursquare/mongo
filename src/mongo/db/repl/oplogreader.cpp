/**
*    Copyright (C) 2012 10gen Inc.
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

#include "mongo/db/oplogreader.h"

#include <boost/shared_ptr.hpp>
#include <string>

#include "mongo/client/dbclientinterface.h"
#include "mongo/db/dbhelpers.h"
#include "mongo/db/jsobj.h"
#include "mongo/db/repl/rs.h"  // theReplSet
#include "mongo/util/assert_util.h"
#include "mongo/util/log.h"

namespace mongo {


    static const BSONObj userReplQuery = fromjson("{\"user\":\"repl\"}");

    bool replHandshake(DBClientConnection *conn, const BSONObj& me) {
        string myname = getHostName();

        BSONObjBuilder cmd;
        cmd.appendAs( me["_id"] , "handshake" );
        if (theReplSet) {
            cmd.append("member", theReplSet->selfId());
            cmd.append("config", theReplSet->myConfig().asBson());
        }

        BSONObj res;
        bool ok = conn->runCommand( "admin" , cmd.obj() , res );
        // ignoring for now on purpose for older versions
        LOG( ok ? 1 : 0 ) << "replHandshake res not: " << ok << " res: " << res << endl;
        return true;
    }

    OplogReader::OplogReader() {
        _tailingQueryOptions = QueryOption_SlaveOk;
        _tailingQueryOptions |= QueryOption_CursorTailable | QueryOption_OplogReplay;
        
        /* TODO: slaveOk maybe shouldn't use? */
        _tailingQueryOptions |= QueryOption_AwaitData;

    }

    bool OplogReader::commonConnect(const string& hostName) {
        if( conn() == 0 ) {
            _conn = shared_ptr<DBClientConnection>(new DBClientConnection(false,
                                                                          0,
                                                                          30));
            string errmsg;
            if ( !_conn->connect(hostName.c_str(), errmsg) ) {
                resetConnection();
                log() << "repl: " << errmsg << endl;
                return false;
            }
        }
        return true;
    }

    bool OplogReader::connect(const std::string& hostName) {
        if (conn()) {
            return true;
        }

        if (!commonConnect(hostName)) {
            return false;
        }

        return true;
    }

    bool OplogReader::connect(const std::string& hostName, const BSONObj& me) {
        if (conn()) {
            return true;
        }

        if (!commonConnect(hostName)) {
            return false;
        }

        if (!replHandshake(_conn.get(), me)) {
            return false;
        }

        return true;
    }

    bool OplogReader::connect(const mongo::OID& rid, const int from, const string& to) {
        if (conn() != 0) {
            return true;
        }
        if (commonConnect(to)) {
            log() << "handshake between " << from << " and " << to << endl;
            return passthroughHandshake(rid, from);
        }
        return false;
    }

    bool OplogReader::passthroughHandshake(const mongo::OID& rid, const int nextOnChainId) {
        BSONObjBuilder cmd;
        cmd.append("handshake", rid);
        if (theReplSet) {
            const Member* chainedMember = theReplSet->findById(nextOnChainId);
            if (chainedMember != NULL) {
                cmd.append("config", chainedMember->config().asBson());
            }
        }
        cmd.append("member", nextOnChainId);

        BSONObj res;
        return conn()->runCommand("admin", cmd.obj(), res);
    }

    void OplogReader::query(const char *ns,
                            Query query,
                            int nToReturn,
                            int nToSkip,
                            const BSONObj* fields) {
        cursor.reset(
            _conn->query(ns, query, nToReturn, nToSkip, fields, QueryOption_SlaveOk).release()
        );
    }

    void OplogReader::tailingQuery(const char *ns, const BSONObj& query, const BSONObj* fields ) {
        verify( !haveCursor() );
        LOG(2) << "repl: " << ns << ".find(" << query.toString() << ')' << endl;
        cursor.reset( _conn->query( ns, query, 0, 0, fields, _tailingQueryOptions ).release() );
    }

    void OplogReader::tailingQueryGTE(const char *ns, OpTime optime, const BSONObj* fields ) {
        BSONObjBuilder gte;
        gte.appendTimestamp("$gte", optime.asDate());
        BSONObjBuilder query;
        query.append("ts", gte.done());
        tailingQuery(ns, query.done(), fields);
    }

}
