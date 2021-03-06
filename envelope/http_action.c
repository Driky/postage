#include "http_action.h"

void http_action_step1(struct sock_ev_client *client) {
	SDEFINE_VAR_ALL(str_uri, str_uri_temp, str_action_name, str_temp, str_args, str_sql);
	char *str_response = NULL;
	char *ptr_end_uri = NULL;
	ssize_t int_len = 0;
	size_t int_uri_len = 0;
	size_t int_args_len = 0;
	size_t int_action_name_len = 0;
	size_t int_sql_len = 0;
	size_t int_response_len = 0;

	str_uri = str_uri_path(client->str_request, client->int_request_len, &int_uri_len);
	SFINISH_CHECK(str_uri != NULL, "str_uri_path failed");
	ptr_end_uri = strchr(str_uri, '?');
	if (ptr_end_uri != NULL) {
		*ptr_end_uri = 0;
		ptr_end_uri = strchr(ptr_end_uri + 1, '#');
		if (ptr_end_uri != NULL) {
			*ptr_end_uri = 0;
		}
	}

	str_args = query(client->str_request, client->int_request_len, &int_args_len);
	if (str_args == NULL) {
		SFINISH_SNCAT(str_args, &int_args_len,
			"", (size_t)0);
	}

#ifdef ENVELOPE
#else
	if (isdigit(str_uri[9])) {
		str_uri_temp = str_uri;
		char *ptr_temp = strchr(str_uri_temp + 9, '/');
		SFINISH_CHECK(ptr_temp != NULL, "strchr failed");
		SFINISH_SNCAT(str_uri, &int_uri_len,
			"/postage/app", (size_t)12,
			ptr_temp, strlen(ptr_temp));
		SFREE(str_uri_temp);
	}
#endif

	SDEBUG("str_args: %s", str_args);

	str_temp = str_args;
	str_args = DB_escape_literal(client->conn, str_temp, int_args_len);
	int_args_len = strlen(str_args);
	SFINISH_CHECK(str_args != NULL, "DB_escape_literal failed");

	SDEBUG("str_args: %s", str_args);

#ifdef ENVELOPE
	SFINISH_SNCAT(str_action_name, &int_action_name_len,
		str_uri + strlen("/env/"), strlen(str_uri + strlen("/env/")));
#else
	SFINISH_SNCAT(str_action_name, &int_action_name_len,
		str_uri + strlen("/postage/app/"), strlen(str_uri + strlen("/postage/app/")));
#endif
	if (DB_connection_driver(client->conn) == DB_DRIVER_POSTGRES) {
		SFINISH_SNCAT(str_sql, &int_sql_len,
			"SELECT ", (size_t)7,
			str_action_name, int_action_name_len,
			"(", (size_t)1,
			str_args, int_args_len,
			");", (size_t)2);
	} else {
		SFINISH_SNCAT(str_sql, &int_sql_len,
			"EXECUTE ", (size_t)8,
			str_action_name, int_action_name_len,
			" ", (size_t)1,
			str_args, int_args_len,
			";", (size_t)1);
	}
	SFINISH_CHECK(query_is_safe(str_sql), "SQL Injection detected");
	SFINISH_CHECK(DB_exec(global_loop, client->conn, client, str_sql, http_action_step2), "DB_exec failed");
	SDEBUG("str_sql: %s", str_sql);

	bol_error_state = false;
finish:
	SFREE_ALL();
	if (bol_error_state) {
		char *_str_response = str_response;
		SFINISH_SNCAT(str_response, &int_response_len,
			"HTTP/1.1 500 Internal Server Error\015\012"
			"Server: " SUN_PROGRAM_LOWER_NAME "\015\012"
			"Connection: close\015\012",
			strlen("HTTP/1.1 500 Internal Server Error\015\012"
				"Server: " SUN_PROGRAM_LOWER_NAME "\015\012"
				"Connection: close\015\012"),
			bol_global_allow_origin ? "Access-Control-Allow-Origin: *\015\012\015\012" : "\015\012",
			strlen(bol_global_allow_origin ? "Access-Control-Allow-Origin: *\015\012\015\012" : "\015\012"),
			_str_response, strlen(_str_response));
		SFREE(_str_response);
	}
	if (str_response != NULL && (int_len = client_write(client, str_response, strlen(str_response))) < 0) {
		SERROR_NORESPONSE("client_write() failed");
	}
	SFREE(str_response);
	if (int_len != 0) {
		ev_io_stop(global_loop, &client->io);
		SFREE(client->str_request);
		SERROR_CHECK_NORESPONSE(client_close(client), "Error closing Client");
	}
}

bool http_action_step2(EV_P, void *cb_data, DB_result *res) {
	struct sock_ev_client_copy_io *client_copy_io = NULL;
	struct sock_ev_client_copy_check *client_copy_check = NULL;
	struct sock_ev_client *client = cb_data;
	char *str_response = NULL;
	char *_str_response = NULL;
	ssize_t int_len = 0;
	size_t int_response_len = 0;
	size_t _int_response_len = 0;
	DArray *arr_row_values = NULL;
	DArray *arr_row_lengths = NULL;

	SFINISH_CHECK(res != NULL, "DB_exec failed, res == NULL");
	SFINISH_CHECK(res->status == DB_RES_TUPLES_OK, "DB_exec failed, res->status == %d", res->status);

	SFINISH_CHECK(DB_fetch_row(res) == DB_FETCH_OK, "DB_fetch_row failed");
	arr_row_values = DB_get_row_values(res);
	SFINISH_CHECK(arr_row_values != NULL, "DB_get_row_values failed");
	arr_row_lengths = DB_get_row_lengths(res);
	SFINISH_CHECK(arr_row_lengths != NULL, "DB_get_row_lengths failed");

	_str_response = DArray_get(arr_row_values, 0);
	_int_response_len = (size_t)(*(ssize_t *)DArray_get(arr_row_lengths, 0));
	if (_str_response == NULL) {
		SFINISH_SNCAT(_str_response, &_int_response_len, "null", 4);
	}
	SFINISH_SNCAT(str_response, &int_response_len,
		"HTTP/1.1 200 OK\015\012"
		"Server: " SUN_PROGRAM_LOWER_NAME "\015\012"
		"Content-Type: application/json; charset=UTF-8\015\012"
		"Connection: close\015\012",
		strlen("HTTP/1.1 200 OK\015\012"
			"Server: " SUN_PROGRAM_LOWER_NAME "\015\012"
			"Content-Type: application/json; charset=UTF-8\015\012"
			"Connection: close\015\012"),
		bol_global_allow_origin ? "Access-Control-Allow-Origin: *\015\012\015\012" : "\015\012",
		strlen(bol_global_allow_origin ? "Access-Control-Allow-Origin: *\015\012\015\012" : "\015\012"),
		"{\"stat\":true, \"dat\": ",
		strlen("{\"stat\":true, \"dat\": "),
		_str_response, _int_response_len,
		"}", (size_t)1);
	SFREE(_str_response);
	SDEBUG("str_response: %s", str_response);

	client->cur_request = create_request(client, NULL, NULL, NULL, NULL, 0, POSTAGE_REQ_ACTION, NULL);
	SFINISH_CHECK(client->cur_request != NULL, "create_request failed!");
	SFINISH_SALLOC(client_copy_check, sizeof(struct sock_ev_client_copy_check));

	client_copy_check->str_response = str_response;
	str_response = NULL;
	client_copy_check->int_response_len = (ssize_t)int_response_len;
	client_copy_check->client_request = client->cur_request;

	SFINISH_SALLOC(client_copy_io, sizeof(struct sock_ev_client_copy_io));

	client_copy_io->client_copy_check = client_copy_check;

	ev_io_init(&client_copy_io->io, http_action_step3, GET_CLIENT_SOCKET(client), EV_WRITE);
	ev_io_start(EV_A, &client_copy_io->io);

	bol_error_state = false;
finish:
	if (arr_row_values != NULL) {
		// the value in this array is used as str_response in the struct
		DArray_destroy(arr_row_values);
	}
	if (arr_row_lengths != NULL) {
		// we copy the length into the struct, so we can free it in the array
		DArray_clear_destroy(arr_row_lengths);
	}
	SFREE(_str_response);
	_str_response = str_response;
	if (bol_error_state) {
		bol_error_state = false;
		SFINISH_SNCAT(str_response, &int_response_len,
			"HTTP/1.1 500 Internal Server Error\015\012"
			"Server: " SUN_PROGRAM_LOWER_NAME "\015\012"
			"Connection: close\015\012",
			strlen("HTTP/1.1 500 Internal Server Error\015\012"
				"Server: " SUN_PROGRAM_LOWER_NAME "\015\012"
				"Connection: close\015\012"),
			bol_global_allow_origin ? "Access-Control-Allow-Origin: *\015\012\015\012" : "\015\012",
			strlen(bol_global_allow_origin ? "Access-Control-Allow-Origin: *\015\012\015\012" : "\015\012"),
			_str_response, strlen(_str_response));
		SFREE(_str_response);
		_str_response = DB_get_diagnostic(client->conn, res);
		SFINISH_SNFCAT(str_response, &int_response_len,
			":\n", (size_t)2,
			_str_response, strlen(_str_response));
		SFREE(_str_response);
	}
	if (str_response != NULL && (int_len = client_write(client, str_response, strlen(str_response))) < 0) {
		SERROR_NORESPONSE("client_write() failed");
	}
	DB_free_result(res);
	SFREE(str_response);
	if (int_len != 0) {
		ev_io_stop(EV_A, &client->io);
		SFREE(client->str_request);
		SERROR_CHECK_NORESPONSE(client_close(client), "Error closing Client");
	}
	return true;
}

void http_action_step3(EV_P, ev_io *w, int revents) {
	if (revents != 0) {
	} // get rid of unused parameter warning
	struct sock_ev_client_copy_io *client_copy_io = (struct sock_ev_client_copy_io *)w;
	struct sock_ev_client_copy_check *client_copy_check = client_copy_io->client_copy_check;
	struct sock_ev_client_request *client_request = client_copy_check->client_request;
//	struct sock_ev_client *client = client_request->parent;
	char *str_response = NULL;
	char *_str_response = NULL;
	size_t int_response_len = 0;

	// SDEBUG("client_copy_check->str_response: %s", client_copy_check->str_response);

	ssize_t int_client_write_len =
		client_write(client_request->parent, client_copy_check->str_response + client_copy_check->int_written,
		(size_t)(client_copy_check->int_response_len - client_copy_check->int_written));

	SDEBUG("write(%i, %p, %i): %z", client_request->parent->int_sock,
		client_copy_check->str_response + client_copy_check->int_written,
		client_copy_check->int_response_len - client_copy_check->int_written, int_client_write_len);

	if (int_client_write_len == SOCK_WANT_READ) {
		ev_io_stop(EV_A, w);
		ev_io_set(w, GET_CLIENT_SOCKET(client_request->parent), EV_READ);
		ev_io_start(EV_A, w);
		bol_error_state = false;
		errno = 0;
		return;

	} else if (int_client_write_len == SOCK_WANT_WRITE) {
		ev_io_stop(EV_A, w);
		ev_io_set(w, GET_CLIENT_SOCKET(client_request->parent), EV_WRITE);
		ev_io_start(EV_A, w);
		bol_error_state = false;
		errno = 0;
		return;

	} else if (int_client_write_len == -1 && errno != EAGAIN) {
		SFINISH("client_write(%i, %p, %i) failed: %i", client_request->parent->int_sock,
			client_copy_check->str_response + client_copy_check->int_written,
			client_copy_check->int_response_len - client_copy_check->int_written, int_client_write_len);
	} else {
		// int_client_write_len can't be negative at this point
		client_copy_check->int_written += (ssize_t)int_client_write_len;
	}

	if (client_copy_check->int_written == client_copy_check->int_response_len) {
		ev_io_stop(EV_A, w);

		SFREE(client_copy_check->str_response);
		SFREE(client_copy_check);
		SFREE(client_copy_io);

		SDEBUG("DONE");
		struct sock_ev_client *client = client_request->parent;
		SFINISH_CLIENT_CLOSE(client);
		client_request = NULL;
	}

	// If this line is not here, we close the client below
	// then libev calls a function on the socket and crashes and burns on windows
	// so... don't touch
	int_client_write_len = 0;

	bol_error_state = false;
finish:
	_str_response = str_response;
	if (bol_error_state) {
		if (client_copy_check != NULL) {
			ev_io_stop(EV_A, w);
			SFREE(client_copy_check->str_response);
			SFREE(client_copy_check);
			SFREE(client_copy_io);
		}
		str_response = NULL;
		bol_error_state = false;

		SFINISH_SNCAT(str_response, &int_response_len,
			"HTTP/1.1 500 Internal Server Error\015\012"
			"Server: " SUN_PROGRAM_LOWER_NAME "\015\012"
			"Connection: close\015\012",
			strlen("HTTP/1.1 500 Internal Server Error\015\012"
				"Server: " SUN_PROGRAM_LOWER_NAME "\015\012"
				"Connection: close\015\012"),
			bol_global_allow_origin ? "Access-Control-Allow-Origin: *\015\012\015\012" : "\015\012",
			strlen(bol_global_allow_origin ? "Access-Control-Allow-Origin: *\015\012\015\012" : "\015\012"),
			_str_response, strlen(_str_response));
		SFREE(_str_response);
	}
	if (str_response != NULL) {
		int_client_write_len = client_write(client_request->parent, str_response, strlen(str_response));
		SDEBUG("int_client_write_len: %d", int_client_write_len);
		if (int_client_write_len < 0) {
			SERROR_NORESPONSE("client_write() failed");
		}
	}
	SFREE(str_response);
	if (int_client_write_len != 0) {
		ev_io_stop(EV_A, &client_request->parent->io);
		SFREE(client_request->parent->str_request);
		SDEBUG("ERROR");
		SERROR_CHECK_NORESPONSE(client_close(client_request->parent), "Error closing Client");
	}
}
