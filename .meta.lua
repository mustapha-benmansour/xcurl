
---@meta xcurl

---@class xcurl
---@field easy fun():xcurl.easy
---@field multi fun():xcurl.multi
local xcurl={}

---@class xcurl.easy
---@operator call:true?,string?,integer?
---@field url string?
---@field port integer
---@field proxy string?
---@field userpwd string?
---@field proxyuserpwd string?
---@field range string?
---@field timeout integer
---@field infilesize integer
---@field referer string?
---@field ftpport string?
---@field useragent string?
---@field low_speed_limit integer
---@field low_speed_time integer
---@field resume_from integer
---@field cookie string?
---@field httpheader string[]?
---@field sslcert string?
---@field keypasswd string?
---@field crlf integer
---@field quote string[]?
---@field cookiefile string?
---@field sslversion integer
---@field timecondition integer
---@field timevalue integer
---@field customrequest string?
---@field postquote string[]?
---@field header integer
---@field nobody integer
---@field failonerror integer
---@field upload integer
---@field post integer
---@field dirlistonly integer
---@field append integer
---@field netrc integer
---@field followlocation integer
---@field transfertext integer
---@field autoreferer integer
---@field proxyport integer
---@field postfieldsize integer
---@field httpproxytunnel integer
---@field interface string?
---@field krblevel string?
---@field ssl_verifypeer integer
---@field cainfo string?
---@field maxredirs integer
---@field filetime integer
---@field telnetoptions string[]?
---@field maxconnects integer
---@field obsolete72 integer
---@field fresh_connect integer
---@field forbid_reuse integer
---@field connecttimeout integer
---@field httpget integer
---@field ssl_verifyhost integer
---@field cookiejar string?
---@field ssl_cipher_list string?
---@field http_version integer
---@field ftp_use_epsv integer
---@field sslcerttype string?
---@field sslkey string?
---@field sslkeytype string?
---@field sslengine string?
---@field sslengine_default integer
---@field dns_cache_timeout integer
---@field prequote string[]?
---@field cookiesession integer
---@field capath string?
---@field buffersize integer
---@field nosignal integer
---@field proxytype integer
---@field accept_encoding string?
---@field http200aliases string[]?
---@field unrestricted_auth integer
---@field ftp_use_eprt integer
---@field httpauth integer
---@field ftp_create_missing_dirs integer
---@field proxyauth integer
---@field server_response_timeout integer
---@field ipresolve integer
---@field maxfilesize integer
---@field infilesize_large integer
---@field resume_from_large integer
---@field maxfilesize_large integer
---@field netrc_file string?
---@field use_ssl integer
---@field postfieldsize_large integer
---@field tcp_nodelay integer
---@field ftpsslauth integer
---@field ftp_account string?
---@field cookielist string?
---@field ignore_content_length integer
---@field ftp_skip_pasv_ip integer
---@field ftp_filemethod integer
---@field localport integer
---@field localportrange integer
---@field connect_only integer
---@field max_send_speed_large integer
---@field max_recv_speed_large integer
---@field ftp_alternative_to_user string?
---@field ssl_sessionid_cache integer
---@field ssh_auth_types integer
---@field ssh_public_keyfile string?
---@field ssh_private_keyfile string?
---@field ftp_ssl_ccc integer
---@field timeout_ms integer
---@field connecttimeout_ms integer
---@field http_transfer_decoding integer
---@field http_content_decoding integer
---@field new_file_perms integer
---@field new_directory_perms integer
---@field postredir integer
---@field ssh_host_public_key_md5 string?
---@field proxy_transfer_mode integer
---@field crlfile string?
---@field issuercert string?
---@field address_scope integer
---@field certinfo integer
---@field username string?
---@field password string?
---@field proxyusername string?
---@field proxypassword string?
---@field noproxy string?
---@field tftp_blksize integer
---@field socks5_gssapi_nec integer
---@field ssh_knownhosts string?
---@field mail_from string?
---@field mail_rcpt string[]?
---@field ftp_use_pret integer
---@field rtsp_request integer
---@field rtsp_session_id string?
---@field rtsp_stream_uri string?
---@field rtsp_transport string?
---@field rtsp_client_cseq integer
---@field rtsp_server_cseq integer
---@field wildcardmatch integer
---@field resolve string[]?
---@field tlsauth_username string?
---@field tlsauth_password string?
---@field tlsauth_type string?
---@field transfer_encoding integer
---@field gssapi_delegation integer
---@field dns_servers string?
---@field accepttimeout_ms integer
---@field tcp_keepalive integer
---@field tcp_keepidle integer
---@field tcp_keepintvl integer
---@field ssl_options integer
---@field mail_auth string?
---@field sasl_ir integer
---@field xoauth2_bearer string?
---@field dns_interface string?
---@field dns_local_ip4 string?
---@field dns_local_ip6 string?
---@field login_options string?
---@field ssl_enable_alpn integer
---@field expect_100_timeout_ms integer
---@field proxyheader string[]?
---@field headeropt integer
---@field pinnedpublickey string?
---@field unix_socket_path string?
---@field ssl_verifystatus integer
---@field ssl_falsestart integer
---@field path_as_is integer
---@field proxy_service_name string?
---@field service_name string?
---@field pipewait integer
---@field default_protocol string?
---@field stream_weight integer
---@field tftp_no_options integer
---@field connect_to string[]?
---@field tcp_fastopen integer
---@field keep_sending_on_error integer
---@field proxy_cainfo string?
---@field proxy_capath string?
---@field proxy_ssl_verifypeer integer
---@field proxy_ssl_verifyhost integer
---@field proxy_sslversion integer
---@field proxy_tlsauth_username string?
---@field proxy_tlsauth_password string?
---@field proxy_tlsauth_type string?
---@field proxy_sslcert string?
---@field proxy_sslcerttype string?
---@field proxy_sslkey string?
---@field proxy_sslkeytype string?
---@field proxy_keypasswd string?
---@field proxy_ssl_cipher_list string?
---@field proxy_crlfile string?
---@field proxy_ssl_options integer
---@field pre_proxy string?
---@field proxy_pinnedpublickey string?
---@field abstract_unix_socket string?
---@field suppress_connect_headers integer
---@field request_target string?
---@field socks5_auth integer
---@field ssh_compression integer
---@field timevalue_large integer
---@field happy_eyeballs_timeout_ms integer
---@field haproxyprotocol integer
---@field dns_shuffle_addresses integer
---@field tls13_ciphers string?
---@field proxy_tls13_ciphers string?
---@field disallow_username_in_url integer
---@field doh_url string?
---@field upload_buffersize integer
---@field upkeep_interval_ms integer
---@field http09_allowed integer
---@field altsvc_ctrl integer
---@field altsvc string?
---@field maxage_conn integer
---@field sasl_authzid string?
---@field mail_rcpt_allowfails integer
---@field sslcert_blob string?
---@field sslkey_blob string?
---@field proxy_sslcert_blob string?
---@field proxy_sslkey_blob string?
---@field issuercert_blob string?
---@field proxy_issuercert string?
---@field proxy_issuercert_blob string?
---@field ssl_ec_curves string?
---@field hsts_ctrl integer
---@field hsts string?
---@field aws_sigv4 string?
---@field doh_ssl_verifypeer integer
---@field doh_ssl_verifyhost integer
---@field doh_ssl_verifystatus integer
---@field cainfo_blob string?
---@field proxy_cainfo_blob string?
---@field ssh_host_public_key_sha256 string?
---@field maxlifetime_conn integer
---@field mime_options integer
---@field protocols_str string?
---@field redir_protocols_str string?
---@field ws_options integer
---@field ca_cache_timeout integer
---@field quick_exit integer
---@field haproxy_client_ip string?
---@field server_response_timeout_ms integer
---@field sockoptfunction function?
---@field ssh_keyfunction function?
---@field seekfunction (fun(origin:'set'|'cur'|'end'|integer,offset:integer):boolean)?
---@field opensocketfunction function?
---@field ssl_ctx_function function?
---@field debugfunction function?
---@field headerfunction fun(data:string)?
---@field writefunction fun(data:string)?
---@field readfunction (fun(size:integer):(string|boolean))?
---@field interleavefunction function?
---@field chunk_bgn_function function?
---@field chunk_end_function function?
---@field fnmatch_function function?
---@field closesocketfunction function?
---@field xferinfofunction function?
---@field resolver_start_function function?
---@field trailerfunction function?
---@field hstsreadfunction function?
---@field hstswritefunction function?
---@field ssh_hostkeyfunction function?
---@field prereqfunction function?
xcurl.easy={}

---not imp
---field curlu ptr_object
---field mimepost ptr_object
---field stream_depends ptr_object
---field stream_depends_e ptr_object
---field copypostfields ptr_object
---field private ptr_object
---field share ptr_object
---field obsolete40 ptr_object
---field errorbuffer ptr_object
---field postfields ptr_object
---field stderr ptr_object

---reserved
---field noprogress integer
---field verbose integer

---reserved
---field ssh_hostkeydata ptr
---field prereqdata ptr
---field hstswritedata ptr
---field hstsreaddata ptr
---field trailerdata ptr
---field resolver_start_data ptr
---field chunk_data ptr
---field interleavedata ptr
---field ssh_keydata ptr
---field seekdata ptr
---field sockoptdata ptr
---field ssl_ctx_data ptr
---field debugdata ptr
---field xferinfodata ptr
---field headerdata ptr
---field writedata ptr
---field readdata ptr
---field fnmatch_data ptr
---field closesocketdata ptr
---field opensocketdata ptr

--readonly
---@class xcurl.info
---@field headers fun():string[]|{[string]:string}
---@field effective_url string?
---@field response_code integer
---@field total_time number
---@field namelookup_time number
---@field connect_time number
---@field pretransfer_time number
---@field size_upload number
---@field size_upload_t integer
---@field size_download number
---@field size_download_t integer
---@field speed_download number
---@field speed_download_t integer
---@field speed_upload number
---@field speed_upload_t integer
---@field header_size integer
---@field request_size integer
---@field ssl_verifyresult integer
---@field filetime integer
---@field filetime_t integer
---@field content_length_download number
---@field content_length_download_t integer
---@field content_length_upload number
---@field content_length_upload_t integer
---@field starttransfer_time number
---@field content_type string?
---@field redirect_time number
---@field redirect_count integer
---@field public private string?
---@field http_connectcode integer
---@field httpauth_avail integer
---@field proxyauth_avail integer
---@field os_errno integer
---@field num_connects integer
---@field ssl_engines string[]?
---@field cookielist string[]?
---@field lastsocket integer
---@field ftp_entry_path string?
---@field redirect_url string?
---@field primary_ip string?
---@field appconnect_time number
---@field certinfo ptr
---@field condition_unmet integer
---@field rtsp_session_id string
---@field rtsp_client_cseq integer
---@field rtsp_server_cseq integer
---@field rtsp_cseq_recv integer
---@field primary_port integer
---@field local_ip string?
---@field local_port integer
---@field tls_session ptr
---@field activesocket socket
---@field tls_ssl_ptr ptr
---@field http_version integer
---@field proxy_ssl_verifyresult integer
---@field protocol integer
---@field scheme string?
---@field total_time_t integer
---@field namelookup_time_t integer
---@field connect_time_t integer
---@field pretransfer_time_t integer
---@field starttransfer_time_t integer
---@field redirect_time_t integer
---@field appconnect_time_t integer
---@field retry_after integer
---@field effective_method string?
---@field proxy_error integer
---@field referer string?
---@field cainfo string?
---@field capath string?
---@field xfer_id integer
---@field conn_id integer
---@field queue_time_t integer
---@field used_proxy integer
xcurl.info={}



---@class xcurl.multi:{[xcurl.easy]:fun(ok:true?,err:string?,errno:integer?)}
---@operator call(fun()?):nil
xcurl.multi={}



return xcurl

