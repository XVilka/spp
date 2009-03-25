/* Mini MCMS :: renamed to 'spp'? */

/* Should be dynamic buffer */
static char *cmd_to_str(const char *cmd)
{
	char *out = (char *)malloc(4096);
	int ret = 0, len = 0, outlen = 4096;
	FILE *fd = popen(cmd, "r");
	while(fd) {
		len += ret;
		ret = fread(out+len, 1, 1023, fd);
		if (ret<1) {
			pclose(fd);
			fd = NULL;
		}
		if (ret+1024>outlen) {
			outlen += 4096;
			out = realloc (out, outlen);
		}
	} 
	out[len]='\0';
	return out;
}

TAG_CALLBACK(spp_set)
{
	char *eq, *val = "";
	if (!echo) return 0;
	eq = strchr(buf, ' ');
	if (eq) {
		*eq = '\0';
		val = eq + 1;
	}
	setenv(buf, val, 1);
	return 0;
}

TAG_CALLBACK(spp_get)
{
	char *var = getenv(buf);
	if (!echo) return 0;
	if (var) fprintf(out, "%s", var);
	return 0;
}

TAG_CALLBACK(spp_add)
{
	char res[32];
	char *var, *eq = strchr(buf, ' ');
	int ret = 0;
	if (!echo) return 0;
	if (eq) {
		*eq = '\0';
		var = getenv(buf);
		if (var != NULL)
			ret = atoi(var);
		ret += atoi(eq+1);
		sprintf(res, "%d", ret);
		setenv(buf, res, 1);
	} else { /* syntax error */ }
	return 0;
}

TAG_CALLBACK(spp_sub)
{
	char *eq = strchr(buf, ' ');
	char *var;
	int ret = 0;
	if (!echo) return 0;
	if (eq) {
		*eq = '\0';
		var = getenv(buf);
		if (var == NULL) ret = 0;
		else ret = atoi(var);
		ret -= atoi(eq+1);
		setenv(buf, eq+1, 1);
	} else { /* syntax error */ }
	return 0;
}

// XXX This method needs some love
TAG_CALLBACK(spp_trace)
{
	char b[1024];
	if (!echo) return 0;
	snprintf(b, 1023, "echo '%s' > /dev/stderr", buf);
	system(b);
	return 0;
}

/* TODO: deprecate */
TAG_CALLBACK(spp_echo)
{
	if (!echo) return 0;
	fprintf(out, "%s", buf);
	// TODO: add variable replacement here?? not necessary, done by {{get}}
	return 0;
}

TAG_CALLBACK(spp_error)
{
	fprintf(out, "ERROR: %s", buf);
	exit(1);
	return 0;
}

TAG_CALLBACK(spp_system)
{
	char *str;
	if (!echo) return 0;
	str = cmd_to_str(buf);
	fprintf(out, "%s", str);
	free(str);
	return 0;
}

TAG_CALLBACK(spp_include)
{
	char *incdir;
	if (!echo) return 0;
	incdir = getenv("SPP_INCDIR");
	if (incdir) {
		char *b = strdup(incdir);
		b = realloc(b, strlen(b)+strlen(buf)+3);
		strcat(b, "/");
		strcat(b, buf);
		spp_file(b, out);
	} else spp_file(buf, out);
	return 0;
}

TAG_CALLBACK(spp_if)
{
	char *var = getenv(buf);
	if (var && *var!='0' && *var != '\0')
		echo = 1;
	else echo = 0;
	return 1;
}

/* {{ ifeq $path / }} */
TAG_CALLBACK(spp_ifeq)
{
	char *value = buf;
	char *eq = strchr(buf, ' ');
	if (eq) {
		*eq = '\0';
		value = getenv(value);
		if (value && !strcmp(value, eq+1)) {
			echo = 1;
		} else echo = 0;
	} else {
		echo = 1;
		value = getenv(buf);
		if (value==NULL || *value=='\0')
			echo = 1;
		else echo = 0;
	}
	return 1;
}

TAG_CALLBACK(spp_else)
{
	echo = echo?0:1;
	return 0;
}

TAG_CALLBACK(spp_ifnot)
{
	spp_if(buf, out);
	spp_else(buf, out);
	return 1;
}

TAG_CALLBACK(spp_ifin)
{
	char *var, *ptr;
	if (!echo) return 1;
	ptr = strchr(buf, ' ');
	echo = 0;
	if (ptr) {
		*ptr='\0';
		var = getenv(buf);
		if (strstr(ptr+1, var)) {
			echo = 1;
		}
	}
	return 1;
}

TAG_CALLBACK(spp_endif)
{
	return -1;
}

TAG_CALLBACK(spp_default)
{
	if (!echo) return 0;
	fprintf(out, "\n** invalid command(%s)", buf);
	return 0;
}

static FILE *spp_pipe_fd = NULL;

TAG_CALLBACK(spp_pipe)
{
	spp_pipe_fd = popen(buf, "w");
	return 0;
}

TAG_CALLBACK(spp_endpipe)
{
	/* TODO: Get output here */
	int ret=0, len = 0;
	int outlen = 4096;
	char *str = (char *)malloc(4096);
	do {
		len += ret;
		ret = fread(str+len, 1, 1023, spp_pipe_fd);
		if (ret+1024>outlen) {
			outlen += 4096;
			str = realloc (str, outlen);
		}
	} while (ret>0);
	str[len]='\0';
	fprintf(out, "%s", str);
	if (spp_pipe_fd)
		pclose(spp_pipe_fd);
	spp_pipe_fd = NULL;
	return 0;
}

PUT_CALLBACK(spp_fputs)
{
	if (spp_pipe_fd) {
		fprintf(spp_pipe_fd, "%s", buf);
	} else fprintf(out, "%s", buf);
	return 0;
}

struct Tag spp_tags[] = {
	{ "get", spp_get },
	{ "set", spp_set },
	{ "add", spp_add },
	{ "sub", spp_sub },
	{ "echo", spp_echo },
	{ "error", spp_error },
	{ "trace", spp_trace },
	{ "ifin", spp_ifin },
	{ "ifnot", spp_ifnot },
	{ "ifeq", spp_ifeq },
	{ "if", spp_if },
	{ "else", spp_else },
	{ "endif", spp_endif },
	{ "pipe", spp_pipe },
	{ "endpipe", spp_endpipe },
	{ "include", spp_include },
	{ "system", spp_system },
	{ NULL, spp_default },
	{ NULL }
};

ARG_CALLBACK(spp_arg_i)
{
	setenv("SPP_INCDIR", arg, 1);
	return 0;
}

ARG_CALLBACK(spp_arg_d)
{
	// TODO: Handle error
	char *eq = strchr(arg, '=');
	if (eq) {
		*eq = '\0';
		setenv(arg, eq+1, 1);
	} else setenv(arg, "", 1);
	return 0;
}

struct Arg spp_args[] = {
	{ "-I", "add include directory", 1, spp_arg_i },
	{ "-D", "define value of variable", 1, spp_arg_d },
	{ NULL }
};

struct Proc spp_proc = {
	.name = "spp",
	.tags = (struct Tag **)spp_tags,
	.args = (struct Arg **)spp_args,
	.token = " ",
	.eof = NULL,
	.tag_pre = "<{",
	.tag_post = "}>",
	.chop = 1,
	.fputs = spp_fputs,
	.multiline = NULL,
	.default_echo = 1,
	.tag_begin = 0,
};