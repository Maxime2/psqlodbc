/* File:			qresult.h
 *
 * Description:		See "qresult.c"
 *
 * Comments:		See "notice.txt" for copyright and license information.
 *
 */

#ifndef __QRESULT_H__
#define __QRESULT_H__

#include "psqlodbc.h"

#include "connection.h"
#include "socket.h"
#include "columninfo.h"
#include "tuple.h"

#include	<libpq-fe.h>
typedef ExecStatusType QueryResultCode;
#define	PGRES_FIELDS_OK		100
#define	PGRES_INTERNAL_ERROR	(PGRES_FIELDS_OK + 1)

enum
{
	FQR_FETCHING_TUPLES = 1L	/* is fetching tuples from db */
	,FQR_REACHED_EOF = (1L << 1)	/* reached eof */
	,FQR_HAS_VALID_BASE = (1L << 2)
};

struct QResultClass_
{
	ColumnInfoClass *fields;	/* the Column information */
	ConnectionClass *conn;		/* the connection this result is using
								 * (backend) */
	QResultClass	*next;		/* the following result class */

	/* Stuff for declare/fetch tuples */
	UInt4		num_total_read;	/* the highest absolute postion ever read in + 1 */
	UInt4		count_backend_allocated;/* m(re)alloced count */
	UInt4		num_cached_rows;	/* count of tuples kept in backend_tuples member */
	Int4		fetch_number;	/* 0-based index to the tuple to read next */
	Int4		cursTuple;	/* absolute current position in the servr's cursor used to retrieve tuples from the DB */
	UInt4		move_offset;
	Int4		base;		/* relative position of rowset start in the current data cache(backend_tuples) */

	UInt2		num_fields;	/* number of fields in the result */
	UInt2		num_key_fields;	/* number of key fields in the result */
	UInt4		cache_size;
	UInt4		rowset_size_include_ommitted;
	Int4		recent_processed_row_count;

	QueryResultCode	rstatus;	/* result status */

	char	sqlstate[8];
	char	*message;
	char *cursor_name;	/* The name of the cursor for select
					 * statements */
	char	*command;
	char	*notice;

	TupleField *backend_tuples;	/* data from the backend (the tuple cache) */
	TupleField *tupleField;		/* current backend tuple being retrieved */

	char	pstatus;		/* processing status */
	char	aborted;		/* was aborted ? */
	char	flags;			/* this result contains keyset etc ? */
	char	move_direction;		/* must move before fetching this
						result set */
	UInt4		count_keyset_allocated; /* m(re)alloced count */
	UInt4		num_cached_keys;	/* count of keys kept in backend_keys member */
	KeySet		*keyset;
	Int4		key_base;	/* relative position of rowset start in the current keyset cache */
	UInt4		reload_count;
	UInt2		rb_alloc;	/* count of allocated rollback info */	
	UInt2		rb_count;	/* count of rollback info */	
	Rollback	*rollback;	
	UInt4		ad_alloc;	/* count of allocated added info */
	UInt4		ad_count;	/* count of newly added rows */
	KeySet		*added_keyset;	/* added keyset info */
	TupleField	*added_tuples;	/* added data by myself */
	UInt2		dl_alloc;	/* count of allocated deleted info */	
	UInt2		dl_count;	/* count of deleted info */	
	UInt4		*deleted;	/* deleted index info */
	KeySet		*deleted_keyset;	/* deleted keyset info */
	UInt2		up_alloc;	/* count of allocated updated info */	
	UInt2		up_count;	/* count of updated info */	
	UInt4		*updated;	/* updated index info */
	KeySet		*updated_keyset;	/* uddated keyset info */
	TupleField	*updated_tuples;	/* uddated data by myself */
};

enum {
	 FQR_HASKEYSET	= 1L
	,FQR_WITHHOLD	= (1L << 1)
	,FQR_HOLDPERMANENT = (1L << 2) /* the cursor is alive across transactions */ 
	,FQR_SYNCHRONIZEKEYS = (1L<<3) /* synchronize the keyset range with that of cthe tuples cache */
}; 

#define	QR_haskeyset(self)		(0 != (self->flags & FQR_HASKEYSET))
#define	QR_is_withhold(self)		(0 != (self->flags & FQR_WITHHOLD))
#define	QR_is_permanent(self)		(0 != (self->flags & FQR_HOLDPERMANENT))
#define	QR_synchronize_keys(self)	(0 != (self->flags & FQR_SYNCHRONIZEKEYS))
#define QR_get_fields(self)		(self->fields)


/*	These functions are for retrieving data from the qresult */
#define QR_get_value_backend(self, fieldno)	(self->tupleField[fieldno].value)
#define QR_get_value_backend_row(self, tupleno, fieldno) ((self->backend_tuples + (tupleno * self->num_fields))[fieldno].value)

/*	These functions are used by both manual and backend results */
#define QR_NumResultCols(self)		(CI_get_num_fields(self->fields))
#define QR_NumPublicResultCols(self)	(QR_haskeyset(self) ? (CI_get_num_fields(self->fields) - self->num_key_fields) : CI_get_num_fields(self->fields))
#define QR_get_fieldname(self, fieldno_)	(CI_get_fieldname(self->fields, fieldno_))
#define QR_get_fieldsize(self, fieldno_)	(CI_get_fieldsize(self->fields, fieldno_))
#define QR_get_display_size(self, fieldno_) (CI_get_display_size(self->fields, fieldno_))
#define QR_get_atttypmod(self, fieldno_)	(CI_get_atttypmod(self->fields, fieldno_))
#define QR_get_field_type(self, fieldno_)	(CI_get_oid(self->fields, fieldno_))

/*	These functions are used only for manual result sets */
#define QR_get_num_total_tuples(self)		(QR_once_reached_eof(self) ? (self->num_total_read + self->ad_count) : self->num_total_read)
#define QR_get_num_total_read(self)		(self->num_total_read)
#define QR_get_num_cached_tuples(self)		(self->num_cached_rows)
#define QR_set_field_info(self, field_num, name, adtid, adtsize, relid, attid)  (CI_set_field_info(self->fields, field_num, name, adtid, adtsize, -1, relid, attid))
#define QR_set_field_info_v(self, field_num, name, adtid, adtsize)  (CI_set_field_info(self->fields, field_num, name, adtid, adtsize, -1, 0, 0))

/* status macros */
#define QR_command_successful(self)	(self && !(self->rstatus == PGRES_BAD_RESPONSE || self->rstatus == PGRES_NONFATAL_ERROR || self->rstatus == PGRES_FATAL_ERROR))
#define QR_command_maybe_successful(self) (self && !(self->rstatus == PGRES_BAD_RESPONSE || self->rstatus == PGRES_FATAL_ERROR))
#define QR_command_nonfatal(self)	( self->rstatus == PGRES_NONFATAL_ERROR)
#define QR_set_conn(self, conn_)			( self->conn = conn_ )
#define QR_set_rstatus(self, condition)		( self->rstatus = condition )
#define QR_set_sqlstatus(self, status)		strcpy(self->sqlstatus, status)
#define QR_set_aborted(self, aborted_)		( self->aborted = aborted_)
#define QR_set_haskeyset(self)		(self->flags |= FQR_HASKEYSET)
#define QR_set_synchronize_keys(self)	(self->flags |= FQR_SYNCHRONIZEKEYS)
#define QR_set_no_cursor(self)		(self->flags &= ~(FQR_WITHHOLD | FQR_HOLDPERMANENT))
#define QR_set_withhold(self)		(self->flags |= FQR_WITHHOLD)
#define QR_set_permanent(self)		(self->flags |= FQR_HOLDPERMANENT)
#define	QR_set_reached_eof(self)	(self->pstatus |= FQR_REACHED_EOF)
#define	QR_set_fetching_tuples(self)	(self->pstatus |= FQR_FETCHING_TUPLES)
#define	QR_set_no_fetching_tuples(self)	(self->pstatus &= ~FQR_FETCHING_TUPLES)
#define QR_set_has_valid_base(self)	(self->pstatus |= FQR_HAS_VALID_BASE)
#define QR_set_no_valid_base(self)	(self->pstatus &= ~FQR_HAS_VALID_BASE)
#define	QR_inc_num_cache(self) \
do { \
	self->num_cached_rows++; \
	if (QR_haskeyset(self)) \
		self->num_cached_keys++; \
} while (0)
#define	QR_set_next_in_cache(self, number) \
do { \
	inolog("set the number to %d to read next\n", number); \
	self->fetch_number = number; \
} while (0)
#define	QR_inc_next_in_cache(self) \
do { \
	inolog("increased the number %d", self->fetch_number); \
	self->fetch_number++; \
	inolog("to %d to next read\n", self->fetch_number); \
} while (0)

#define QR_get_message(self)				(self->message)
#define QR_get_command(self)				(self->command)
#define QR_get_notice(self)				(self->notice)
#define QR_get_rstatus(self)				(self->rstatus)
#define QR_get_aborted(self)				(self->aborted)
#define QR_get_conn(self)				(self->conn)
#define QR_get_cursor(self)				(self->cursor_name)
#define QR_get_rowstart_in_cache(self)			(self->base)
#define QR_once_reached_eof(self)	((self->pstatus & FQR_REACHED_EOF) != 0)
#define QR_is_fetching_tuples(self)	((self->pstatus & FQR_FETCHING_TUPLES) != 0)
#define	QR_has_valid_base(self)		(0 != (self->pstatus & FQR_HAS_VALID_BASE))

#define QR_aborted(self)		(!self || self->aborted)
#define QR_get_reqsize(self)		(self->rowset_size_include_ommitted)

#define QR_stop_movement(self)		(self->move_direction = 0)
#define QR_is_moving(self)		(0 != self->move_direction)
#define QR_is_not_moving(self)		(0 == self->move_direction)
#define QR_set_move_forward(self)	(self->move_direction = 1)
#define QR_is_moving_forward(self)	(1 == self->move_direction)
#define QR_set_move_backward(self)	(self->move_direction = -1)
#define QR_is_moving_backward(self)	(-1 == self->move_direction)
#define QR_set_move_from_the_last(self)	(self->move_direction = 2)
#define QR_is_moving_from_the_last(self)	(2 == self->move_direction)
#define QR_is_moving_not_backward(self)	(0 < self->move_direction)

/*	Core Functions */
QResultClass	*QR_Constructor(void);
void		QR_Destructor(QResultClass *self);
TupleField	*QR_AddNew(QResultClass *self);
BOOL		QR_get_tupledata(QResultClass *self, BOOL binary);
int			QR_next_tuple(QResultClass *self, StatementClass *);
int			QR_close(QResultClass *self);
void		QR_close_result(QResultClass *self, BOOL destroy);
char		QR_fetch_tuples(QResultClass *self, ConnectionClass *conn, const char *cursor);
void		QR_free_memory(QResultClass *self);
void		QR_set_command(QResultClass *self, const char *msg);
void		QR_set_message(QResultClass *self, const char *msg);
void		QR_add_message(QResultClass *self, const char *msg);
void		QR_set_notice(QResultClass *self, const char *msg);
void		QR_add_notice(QResultClass *self, const char *msg);

void		QR_set_num_fields(QResultClass *self, int new_num_fields); /* catalog functions' result only */

void		QR_set_num_cached_rows(QResultClass *, int);
void		QR_set_rowstart_in_cache(QResultClass *, int);
void		QR_inc_rowstart_in_cache(QResultClass *self, int base_inc);
void		QR_set_cache_size(QResultClass *self, int cache_size);
void		QR_set_rowset_size(QResultClass *self, int rowset_size);
void		QR_set_position(QResultClass *self, int pos);
void		QR_set_cursor(QResultClass *self, const char *name);
Int4		getNthValid(const QResultClass *self, Int4 sta, UWORD orientation, UInt4 nth, Int4 *nearest);

#define QR_MALLOC_return_with_error(t, tp, s, a, m, r) \
do { \
 	if (t = (tp *) malloc(s), NULL == t) \
	{ \
 		QR_set_rstatus(a, PGRES_FATAL_ERROR); \
 		QR_set_message(a, m); \
 		return r; \
	} \
} while (0)
#define QR_REALLOC_return_with_error(t, tp, s, a, m, r) \
do { \
	if (t = (tp *) realloc(t, s), NULL == t) \
	{ \
 		QR_set_rstatus(a, PGRES_FATAL_ERROR); \
 		QR_set_message(a, m); \
		return r; \
	} \
} while (0)
#endif /* __QRESULT_H__ */
