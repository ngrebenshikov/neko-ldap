#include <stdio.h>
#include <ctype.h>
#include <neko.h>

#define LDAP_DEPRECATED 1
#include <ldap.h>

#define ERROR_MSG_SIZE 1024
#define LDAP_ATTRS_MAX_SIZE 1024

DEFINE_KIND(k_ldap_pointer);
DEFINE_KIND(k_ldap_message);

void nekoldap_error(char * template, char * error_string) {
		char msg[ERROR_MSG_SIZE];
		sprintf(msg, template, error_string);
		failure(msg);
}

value nekoldap_connect(value uri) {
	LDAP *ldap;
	int rc;
	
	val_check(uri, string);
	rc = ldap_initialize(&ldap, val_string(uri));
	if (rc != LDAP_SUCCESS) {
		val_throw(alloc_int(rc));
	} else {
		int protocol_version = 3;
		int referrals = 0;
		ldap_set_option(ldap, LDAP_OPT_PROTOCOL_VERSION, &protocol_version);
		ldap_set_option(ldap, LDAP_OPT_REFERRALS, &referrals);
		return alloc_abstract(k_ldap_pointer, ldap);
	}
}

value nekoldap_set_option(value ldap, value option, value newval) {
	failure("Not implemented");
}

value nekoldap_bind(value ldap, value dn, value password) {
	int rc;

	val_check_kind(ldap, k_ldap_pointer);
	val_check(dn, string);
	val_check(password, string);


	if ((rc = ldap_bind_s(val_data(ldap), val_string(dn), val_string(password), LDAP_AUTH_SIMPLE)) != LDAP_SUCCESS) {
		val_throw(alloc_int(rc));
		return val_false;	
	} else {
		return val_true;
	}
}


value nekoldap_unbind(value ldap) {
	val_check_kind(ldap, k_ldap_pointer);
	return alloc_int(ldap_unbind(val_data(ldap)));
}

value nekoldap_search(value *args, int nargs) {
	int i, rc;

	if (nargs != 6) neko_error();
	
	val_check_kind(args[0], k_ldap_pointer);
	LDAP * ldap  = val_data(args[0]);
	
	val_check(args[1], string);
	char * base = val_string(args[1]);
	
	val_check(args[2], int);
	int scope = val_int(args[2]);
	
	val_check(args[3], string);
	char * filter = val_string(args[3]);
	
	
	val_check(args[4], array);
	value * v_attrs = val_array_ptr(args[4]);
	
	char * ch_attrs[LDAP_ATTRS_MAX_SIZE];
	for (i=0; i<val_array_size(args[4]); i++) {
		val_check(v_attrs[i], string);
		ch_attrs[i] = val_string(v_attrs[i]);
	}
	ch_attrs[i]=0;

	val_check(args[5], int);
	int attrsonly = val_int(args[5]);
	
	
	LDAPMessage *res;
	
	if ((rc = ldap_search_s(ldap, base, scope, filter, ch_attrs, attrsonly, &res)) != LDAP_SUCCESS) {
		val_throw(alloc_int(rc));
	}
	
	return alloc_abstract(k_ldap_message, res);
}

char * strtolower(char * str) {
	char * p = str; 
	while(*p != 0) {
		*p = tolower(*p);
		p++;
	}
	return str;
}

value nekoldap_get_entries(value neko_ldap, value neko_ldap_result) {
	val_check_kind(neko_ldap, k_ldap_pointer);
	val_check_kind(neko_ldap_result, k_ldap_message);

	LDAPMessage *ldap_result = val_data(neko_ldap_result), *ldap_result_entry;
	LDAP *ldap = val_data(neko_ldap);

	value neko_result;
	
	int num_entries, num_values, i;
	BerElement *ber;
	char *attribute;
	struct berval **ldap_value;
	char *dn;
	
	num_entries = ldap_count_entries(ldap, ldap_result);
	neko_result = alloc_array(num_entries);

	if (num_entries == 0) return val_false;
	num_entries = 0;
	
	ldap_result_entry = ldap_first_entry(ldap, ldap_result);
	if (ldap_result_entry == NULL) return val_false;

	while (ldap_result_entry != NULL) {

		value item = alloc_object(NULL);

		attribute = ldap_first_attribute(ldap, ldap_result_entry, &ber);

		while (attribute != NULL) {
			ldap_value = ldap_get_values_len(ldap, ldap_result_entry, attribute);
			num_values = ldap_count_values_len(ldap_value);

			value neko_attribute = alloc_array(num_values);
			for (i = 0; i < num_values; i++) {
				val_array_ptr(neko_attribute)[i] = alloc_string(ldap_value[i]->bv_val);
			}	
			ldap_value_free_len(ldap_value);

			alloc_field(item, val_id(strtolower(attribute)), neko_attribute); 
			ldap_memfree(attribute);
			attribute = ldap_next_attribute(ldap, ldap_result_entry, ber);
		}
		if (ber != NULL) {
			ber_free(ber, 0);
		}

		dn = ldap_get_dn(ldap, ldap_result_entry);
		alloc_field(item, val_id("dn"), alloc_string(dn)); 
		ldap_memfree(dn);
		val_array_ptr(neko_result)[num_entries++] = item; 
		ldap_result_entry = ldap_next_entry(ldap, ldap_result_entry);
	}
	
	return neko_result;
}

value nekoldap_err2string(value result) {
	val_check(result, int);
	return alloc_string(ldap_err2string(val_int(result)));
}

DEFINE_PRIM(nekoldap_connect, 1);
DEFINE_PRIM(nekoldap_bind, 3);
DEFINE_PRIM(nekoldap_unbind, 1);
DEFINE_PRIM_MULT(nekoldap_search);
DEFINE_PRIM(nekoldap_get_entries, 2);
DEFINE_PRIM(nekoldap_err2string, 1);
