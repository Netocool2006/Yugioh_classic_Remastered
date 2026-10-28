// Prueba de la conexion directa (src/red/nat.h, rama classic-red-nat), sin la consola. Casos CU-RED-NAT-*:
//  - STUN con los vectores del RFC 5769 (peticion, respuesta IPv4 e IPv6, FINGERPRINT) y armar/leer ida y vuelta
//  - DNS propio (consulta y respuesta con compresion de nombres)
//  - SSDP y SOAP con respuestas de routers (MiniUPnPd, FRITZ!Box, TP-Link, Huawei de ISP con PPPoE), HTTP "chunked"
//  - UPnP entero contra un router de mentira por TCP en loopback (puerto ocupado 718, mapeo, IP externa, borrar)
//  - codigo manual ida y vuelta y con errores; clasificacion CGNAT/privado/simetrico/cono/abierto
//  - perforacion en loopback (IPv4 e IPv6) y con NAT SIMULADOS EN EL PROCESO (el contenedor no tiene NET_ADMIN ni
//    namespaces): un "internet" que reescribe direcciones como cono completo, restringido por IP, restringido por
//    puerto y simetrico, con servidores STUN, horquilla, CGNAT anidado y perdida de paquetes; la matriz de que conecta
//  - (opcional, "real") STUN contra los servidores publicos: no imprime la IP
// Uso: prueba_red_nat [real]
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <random>
#include <functional>
#include "red/nat.h"
#include "idioma.h"

using namespace red;

static int fallos = 0, casos = 0;
static void caso(const char* id, bool ok, const std::string& detalle = "")
{
	casos++;
	if (!ok) fallos++;
	printf("%s %s %s\n", ok ? "OK   " : "FALLO", id, detalle.c_str());
	fflush(stdout);
}
static std::vector<uint8_t> hex(const char* h)
{
	std::vector<uint8_t> r;
	int v = -1;
	for (const char* p = h; *p; p++) {
		int d = isdigit((unsigned char)*p) ? *p - '0' : (*p >= 'a' && *p <= 'f') ? *p - 'a' + 10 : -1;
		if (d < 0) continue;
		if (v < 0) v = d; else { r.push_back((uint8_t)(v * 16 + d)); v = -1; }
	}
	return r;
}
static Direccion dir(const char* t) { Direccion d; Direccion::desdeTexto(t, d); return d; }

// ================================================================================================ STUN (RFC 5769)
static void pruebaStun()
{
	const auto pet = hex("000100582112a442b7e7a701bc34d686fa87dfae802200105354554e207465737420636c69656e74"
		"002400046e0001ff80290008932ff9b151263b36000600096576746a3a68367659202020"
		"000800149aeaa70cbfd8cb56781ef2b5b2d3f249c1b571a280280004e57a3bcf");
	const auto r4 = hex("010100" "3c2112a442b7e7a701bc34d686fa87dfae8022000b7465737420766563746f7220"
		"002000080001a147e112a643000800142b91f599fd9e90c38c7489f92af9ba53f06be7d780280004c07d4c96");
	const auto r6 = hex("010100482112a442b7e7a701bc34d686fa87dfae8022000b7465737420766563746f7220"
		"00200014" "0002a1470113a9faa5d3f179bc25f4b5bed2b9d9" "00080014a382954e4be67bf11784c97c8292c275bfe3ed41"
		"80280004c8fb0b4c");
	const uint8_t tid[12] = { 0xb7, 0xe7, 0xa7, 0x01, 0xbc, 0x34, 0xd6, 0x86, 0xfa, 0x87, 0xdf, 0xae };
	uint8_t t[12];
	caso("CU-RED-NAT-STUN-RFC5769-PETICION", stun::leerPeticion(pet.data(), pet.size(), t) && memcmp(t, tid, 12) == 0,
		"2.1: Binding Request con FINGERPRINT valido");
	Direccion d;
	bool ok = stun::leerRespuesta(r4.data(), r4.size(), tid, d);
	caso("CU-RED-NAT-STUN-RFC5769-IPV4", ok && d == dir("192.0.2.1:32853"), "2.2: " + (ok ? d.texto() : std::string("no leida")));
	ok = stun::leerRespuesta(r6.data(), r6.size(), tid, d);
	caso("CU-RED-NAT-STUN-RFC5769-IPV6", ok && d == dir("[2001:db8:1234:5678:11:2233:4455:6677]:32853"),
		"2.3: " + (ok ? d.texto() : std::string("no leida")));
	// errores: un bit cambiado (FINGERPRINT), otro id de transaccion, truncada, no STUN
	auto mala = r4;
	mala[30] ^= 1;
	uint8_t otro[12];
	memcpy(otro, tid, 12);
	otro[0] ^= 1;
	caso("CU-RED-NAT-STUN-ERRORES",
		!stun::leerRespuesta(mala.data(), mala.size(), tid, d) && !stun::leerRespuesta(r4.data(), r4.size(), otro, d) &&
		!stun::leerRespuesta(r4.data(), 40, tid, d) && !stun::esStun((const uint8_t*)"YCPF0123456789abcdefghij", 24) &&
		!stun::leerPeticion(r4.data(), r4.size(), t),
		"FINGERPRINT malo, otro id, truncada y paquete del juego: rechazados");
	// armar y leer (IPv4, IPv6) y la peticion de 20 bytes
	uint8_t b[128];
	bool ida = true;
	for (const char* x : { "203.0.113.9:40000", "[2001:db8::42]:1", "1.2.3.4:65535" }) {
		const size_t n = stun::armarRespuesta(b, tid, dir(x));
		ida = ida && stun::leerRespuesta(b, n, tid, d) && d == dir(x);
	}
	const size_t np = stun::armarPeticion(b, tid);
	caso("CU-RED-NAT-STUN-ARMAR", ida && np == 20 && stun::leerPeticion(b, np, t) && stun::esStun(b, np),
		"respuesta con XOR-MAPPED + FINGERPRINT ida y vuelta (v4 y v6), peticion de 20 bytes");
	// servidor viejo (RFC 3489): solo MAPPED-ADDRESS
	const auto vieja = hex("0101000c2112a442b7e7a701bc34d686fa87dfae" "000100080001" "1f90" "c6336407");
	ok = stun::leerRespuesta(vieja.data(), vieja.size(), tid, d);
	caso("CU-RED-NAT-STUN-MAPPED-VIEJO", ok && d == dir("198.51.100.7:8080"), ok ? d.texto() : "no leida");
	// la CRC-32 (vector clasico "123456789" = cbf43926)
	caso("CU-RED-NAT-CRC32", stun::crc32((const uint8_t*)"123456789", 9) == 0xCBF43926u);
}

// ================================================================================================ DNS
static void pruebaDns()
{
	uint8_t b[300];
	const size_t n = dns::armarConsulta(b, sizeof b, 0x1234, "stun.l.google.com", 1);
	const auto esperada = hex("123401000001000000000000" "0473747" "56e016c06676f6f676c6503636f6d00" "00010001");
	caso("CU-RED-NAT-DNS-CONSULTA", n == esperada.size() && memcmp(b, esperada.data(), n) == 0, std::to_string(n) + " bytes");
	// respuesta: la pregunta, un CNAME (con puntero) y dos A (con puntero al CNAME)
	const auto r = hex("1234818000010003000000000473747" "56e016c06676f6f676c6503636f6d0000010001"
		"c00c000500010000012c0008" "0473747" "56e31c011"            // stun.l.google.com CNAME stun1.l.google.com
		"c02f00010001000000780004" "4a7dfa81"                      // A 74.125.250.129
		"c02f00010001000000780004" "c0000201");                    // A 192.0.2.1
	std::vector<Direccion> out;
	const bool ok = dns::leerRespuesta(r.data(), r.size(), 0x1234, 1, 19302, out);
	caso("CU-RED-NAT-DNS-RESPUESTA", ok && out.size() == 2 && out[0] == dir("74.125.250.129:19302") && out[1] == dir("192.0.2.1:19302"),
		ok ? std::to_string(out.size()) + " direcciones" : "no leida");
	auto mala = r;
	mala[3] = 0x83;   // NXDOMAIN
	std::vector<Direccion> o2;
	caso("CU-RED-NAT-DNS-ERRORES", !dns::leerRespuesta(mala.data(), mala.size(), 0x1234, 1, 1, o2) &&
		!dns::leerRespuesta(r.data(), r.size(), 0x9999, 1, 1, o2) && !dns::leerRespuesta(r.data(), 40, 0x1234, 1, 1, o2),
		"NXDOMAIN, otro id y truncada");
	// la lista de servidores STUN (archivo de datos y la de respaldo)
	const auto lista = servidoresStun("");
	caso("CU-RED-NAT-STUN-LISTA", lista.size() >= 6 && !lista[0].respaldo.empty() && lista[0].puerto == 19302,
		std::to_string(lista.size()) + " servidores");
}

// ================================================================================================ SSDP / SOAP
static const char* SSDP_MINIUPNPD =
	"HTTP/1.1 200 OK\r\nCACHE-CONTROL: max-age=120\r\nST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n"
	"USN: uuid:3e4c5f2a-1d2b-4c6e-8a9f-0123456789ab::urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\nEXT:\r\n"
	"SERVER: OpenWRT/23.05.3 UPnP/1.1 MiniUPnPd/2.3.4\r\nLOCATION: http://192.168.1.1:5000/rootDesc.xml\r\n"
	"OPT: \"http://schemas.upnp.org/upnp/1/0/\"; ns=01\r\n01-NLS: 1712345678\r\nBOOTID.UPNP.ORG: 1712345678\r\n"
	"CONFIGID.UPNP.ORG: 1337\r\n\r\n";
static const char* SSDP_FRITZ =
	"HTTP/1.1 200 OK\r\nLOCATION: http://192.168.178.1:49000/igddesc.xml\r\nSERVER: FRITZ!Box 7590 UPnP/1.0 AVM FRITZ!Box 7590 154.07.57\r\n"
	"CACHE-CONTROL: max-age=1800\r\nEXT:\r\nST: urn:schemas-upnp-org:service:WANIPConnection:1\r\n"
	"USN: uuid:76802409-bccb-40e7-8e6b-3431C4D4E5F6::urn:schemas-upnp-org:service:WANIPConnection:1\r\n\r\n";
static const char* SSDP_TPLINK =   // cabeceras en minusculas y mezcladas
	"HTTP/1.1 200 OK\r\nCache-Control: max-age=100\r\nDate: Wed, 21 Oct 2026 10:00:00 GMT\r\nExt: \r\n"
	"Location: http://192.168.0.1:1900/gatedesc.xml\r\nServer: ipos/7.0 UPnP/1.0 TL-WR840N/1.0\r\n"
	"st: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\nusn: uuid:9f0865b3-f5da-4ad5-85b7-7404637fdf37::urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n\r\n";
static const char* SSDP_TELE =   // un televisor (no es pasarela): se ignora
	"HTTP/1.1 200 OK\r\nCACHE-CONTROL: max-age=1800\r\nLOCATION: http://192.168.1.40:9197/dmr\r\nST: urn:schemas-upnp-org:device:MediaRenderer:1\r\n"
	"USN: uuid:0a1b::urn:schemas-upnp-org:device:MediaRenderer:1\r\n\r\n";

// descripcion de MiniUPnPd (recortada a la estructura real: WANDevice > WANConnectionDevice > WANIPConnection:1)
static const char* DESC_MINIUPNPD =
	"<?xml version=\"1.0\"?>\r\n<root xmlns=\"urn:schemas-upnp-org:device-1-0\" configId=\"1337\"><specVersion><major>1</major><minor>1</minor></specVersion>"
	"<device><deviceType>urn:schemas-upnp-org:device:InternetGatewayDevice:1</deviceType><friendlyName>OpenWrt router</friendlyName>"
	"<manufacturer>OpenWrt</manufacturer><serviceList><service><serviceType>urn:schemas-upnp-org:service:Layer3Forwarding:1</serviceType>"
	"<serviceId>urn:upnp-org:serviceId:L3Forwarding1</serviceId><SCPDURL>/L3F.xml</SCPDURL><controlURL>/ctl/L3F</controlURL>"
	"<eventSubURL>/evt/L3F</eventSubURL></service></serviceList><deviceList><device>"
	"<deviceType>urn:schemas-upnp-org:device:WANDevice:1</deviceType><serviceList><service>"
	"<serviceType>urn:schemas-upnp-org:service:WANCommonInterfaceConfig:1</serviceType><serviceId>urn:upnp-org:serviceId:WANCommonIFC1</serviceId>"
	"<SCPDURL>/WANCfg.xml</SCPDURL><controlURL>/ctl/CmnIfCfg</controlURL><eventSubURL>/evt/CmnIfCfg</eventSubURL></service></serviceList>"
	"<deviceList><device><deviceType>urn:schemas-upnp-org:device:WANConnectionDevice:1</deviceType><serviceList><service>"
	"<serviceType>urn:schemas-upnp-org:service:WANIPConnection:1</serviceType><serviceId>urn:upnp-org:serviceId:WANIPConn1</serviceId>"
	"<SCPDURL>/WANIPCn.xml</SCPDURL><controlURL>/ctl/IPConn</controlURL><eventSubURL>/evt/IPConn</eventSubURL></service>"
	"</serviceList></device></deviceList></device></deviceList><presentationURL>http://192.168.1.1/</presentationURL></device></root>";
// FRITZ!Box: IGD:2 con WANIPConnection:2 y :1 y WANPPPConnection:1 (rutas propias)
static const char* DESC_FRITZ =
	"<?xml version=\"1.0\"?>\n<root xmlns=\"urn:schemas-upnp-org:device-1-0\">\n<specVersion>\n<major>1</major>\n<minor>0</minor>\n</specVersion>\n"
	"<device>\n<deviceType>urn:schemas-upnp-org:device:InternetGatewayDevice:2</deviceType>\n<friendlyName>FRITZ!Box 7590</friendlyName>\n"
	"<deviceList>\n<device>\n<deviceType>urn:schemas-upnp-org:device:WANDevice:2</deviceType>\n<deviceList>\n<device>\n"
	"<deviceType>urn:schemas-upnp-org:device:WANConnectionDevice:2</deviceType>\n<serviceList>\n"
	"<service>\n<serviceType>urn:schemas-upnp-org:service:WANPPPConnection:1</serviceType>\n<serviceId>urn:upnp-org:serviceId:WANPPPConn1</serviceId>\n"
	"<controlURL>/igd2upnp/control/WANPPPConn1</controlURL>\n<eventSubURL>/igd2upnp/control/WANPPPConn1</eventSubURL>\n<SCPDURL>/igd2wanpppconnSCPD.xml</SCPDURL>\n</service>\n"
	"<service>\n<serviceType>urn:schemas-upnp-org:service:WANIPConnection:1</serviceType>\n<serviceId>urn:upnp-org:serviceId:WANIPConn1</serviceId>\n"
	"<controlURL>/igd2upnp/control/WANIPConn1</controlURL>\n<eventSubURL>/igd2upnp/control/WANIPConn1</eventSubURL>\n<SCPDURL>/igd2wanipconnSCPD.xml</SCPDURL>\n</service>\n"
	"<service>\n<serviceType>urn:schemas-upnp-org:service:WANIPConnection:2</serviceType>\n<serviceId>urn:upnp-org:serviceId:WANIPConn1</serviceId>\n"
	"<controlURL>/igd2upnp/control/WANIPConn2</controlURL>\n<eventSubURL>/igd2upnp/control/WANIPConn2</eventSubURL>\n<SCPDURL>/igd2wanipconn2SCPD.xml</SCPDURL>\n</service>\n"
	"</serviceList>\n</device>\n</deviceList>\n</device>\n</deviceList>\n</device>\n</root>\n";
// router de ISP con PPPoE (estilo Huawei HG8245): solo WANPPPConnection:1, URLBase y controlURL relativa sin '/'
static const char* DESC_PPP =
	"<?xml version=\"1.0\" encoding=\"utf-8\"?><root xmlns=\"urn:schemas-upnp-org:device-1-0\"><URLBase>http://192.168.100.1:37215/</URLBase>"
	"<device><deviceType>urn:schemas-upnp-org:device:InternetGatewayDevice:1</deviceType><friendlyName>HG8245H</friendlyName>"
	"<deviceList><device><deviceType>urn:schemas-upnp-org:device:WANDevice:1</deviceType><deviceList><device>"
	"<deviceType>urn:schemas-upnp-org:device:WANConnectionDevice:1</deviceType><serviceList><service>"
	"<serviceType>urn:schemas-upnp-org:service:WANPPPConnection:1</serviceType><serviceId>urn:upnp-org:serviceId:WANPPPConn1</serviceId>"
	"<controlURL>upnp/control/WANPPPConn1</controlURL><eventSubURL>upnp/event/WANPPPConn1</eventSubURL><SCPDURL>wanpppcfg.xml</SCPDURL>"
	"</service></serviceList></device></deviceList></device></deviceList></device></root>";

static const char* SOAP_IP_MINIUPNPD =
	"HTTP/1.1 200 OK\r\nContent-Type: text/xml; charset=\"utf-8\"\r\nConnection: close\r\nContent-Length: 321\r\nServer: OpenWRT/23.05.3 UPnP/1.1 MiniUPnPd/2.3.4\r\n"
	"Ext:\r\n\r\n<?xml version=\"1.0\"?>\r\n<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
	"<s:Body><u:GetExternalIPAddressResponse xmlns:u=\"urn:schemas-upnp-org:service:WANIPConnection:1\"><NewExternalIPAddress>203.0.113.77</NewExternalIPAddress>"
	"</u:GetExternalIPAddressResponse></s:Body></s:Envelope>\r\n";
static const char* FALLA_718_CUERPO =
	"<?xml version=\"1.0\"?>\r\n<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
	"<s:Body><s:Fault><faultcode>s:Client</faultcode><faultstring>UPnPError</faultstring><detail><UPnPError xmlns=\"urn:schemas-upnp-org:control-1-0\">"
	"<errorCode>718</errorCode><errorDescription>ConflictInMappingEntry</errorDescription></UPnPError></detail></s:Fault></s:Body></s:Envelope>\r\n";
static const char* SOAP_IP_PREFIJO =   // estilo AVM: prefijo en los argumentos de salida y CGNAT
	"HTTP/1.1 200 OK\r\nCONTENT-LENGTH: 296\r\nCONTENT-TYPE: text/xml; charset=\"utf-8\"\r\nSERVER: FRITZ!Box 7590 UPnP/1.0 AVM FRITZ!Box 7590 154.07.57\r\n\r\n"
	"<?xml version=\"1.0\"?>\n<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\n"
	"<s:Body>\n<u:GetExternalIPAddressResponse xmlns:u=\"urn:schemas-upnp-org:service:WANIPConnection:1\">\n<u:NewExternalIPAddress>100.72.14.3</u:NewExternalIPAddress>\n"
	"</u:GetExternalIPAddressResponse>\n</s:Body>\n</s:Envelope>\n";

static std::string conLargoReal(std::string r)   // ajusta Content-Length al cuerpo (las de arriba estan escritas a mano)
{
	const size_t fc = r.find("\r\n\r\n");
	const std::string cuerpo = r.substr(fc + 4);
	for (const char* nombre : { "Content-Length: ", "CONTENT-LENGTH: " }) {
		const size_t p = r.find(nombre);
		if (p == std::string::npos || p > fc) continue;
		const size_t e = r.find("\r\n", p);
		r = r.substr(0, p) + nombre + std::to_string(cuerpo.size()) + r.substr(e);
	}
	return r;
}
static std::string chunked(const std::string& cuerpo, const std::string& cab)
{
	std::string r = cab + "Transfer-Encoding: chunked\r\n\r\n";
	for (size_t i = 0; i < cuerpo.size(); i += 100) {
		const std::string t = cuerpo.substr(i, 100);
		char h[24];
		snprintf(h, sizeof h, "%zx", t.size());
		r += std::string(h) + "\r\n" + t + "\r\n";
	}
	return r + "0\r\n\r\n";
}

static std::string falla718()   // MiniUPnPd: 500 con la falla SOAP, en trozos
{
	return chunked(FALLA_718_CUERPO, "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/xml; charset=\"utf-8\"\r\nConnection: close\r\n");
}

static void pruebaSsdpSoap()
{
	std::string u1, u2, u3, u4;
	caso("CU-RED-NAT-SSDP-ROUTERS",
		upnp::leerRespuestaSsdp(SSDP_MINIUPNPD, u1) && u1 == "http://192.168.1.1:5000/rootDesc.xml" &&
		upnp::leerRespuestaSsdp(SSDP_FRITZ, u2) && u2 == "http://192.168.178.1:49000/igddesc.xml" &&
		upnp::leerRespuestaSsdp(SSDP_TPLINK, u3) && u3 == "http://192.168.0.1:1900/gatedesc.xml" && !upnp::leerRespuestaSsdp(SSDP_TELE, u4),
		"MiniUPnPd, FRITZ!Box, TP-Link (minusculas) y un televisor (ignorado)");
	const std::string m = upnp::armarBusqueda("urn:schemas-upnp-org:device:InternetGatewayDevice:1");
	caso("CU-RED-NAT-SSDP-BUSQUEDA", m.find("M-SEARCH * HTTP/1.1\r\n") == 0 && m.find("MAN: \"ssdp:discover\"") != std::string::npos &&
		m.find("HOST: 239.255.255.250:1900") != std::string::npos && m.substr(m.size() - 4) == "\r\n\r\n");
	upnp::Url u;
	upnp::leerUrl(u1, u);
	auto s1 = upnp::leerDescripcion(DESC_MINIUPNPD, u);
	caso("CU-RED-NAT-UPNP-DESC-MINIUPNPD", s1.size() == 1 && s1[0].tipo == "urn:schemas-upnp-org:service:WANIPConnection:1" &&
		s1[0].control.dir == dir("192.168.1.1:5000") && s1[0].control.ruta == "/ctl/IPConn",
		s1.empty() ? "sin servicios" : s1[0].control.anfitrion() + s1[0].control.ruta);
	upnp::leerUrl(u2, u);
	auto s2 = upnp::leerDescripcion(DESC_FRITZ, u);
	caso("CU-RED-NAT-UPNP-DESC-FRITZ", s2.size() == 3 && s2[0].tipo.find("WANIPConnection:2") != std::string::npos &&
		s2[1].tipo.find("WANIPConnection:1") != std::string::npos && s2[2].tipo.find("WANPPPConnection:1") != std::string::npos &&
		s2[0].control.ruta == "/igd2upnp/control/WANIPConn2", "orden: IP:2, IP:1, PPP:1");
	upnp::leerUrl("http://192.168.100.1:37215/upnpdev.xml", u);
	auto s3 = upnp::leerDescripcion(DESC_PPP, u);
	caso("CU-RED-NAT-UPNP-DESC-PPPOE", s3.size() == 1 && s3[0].control.dir == dir("192.168.100.1:37215") &&
		s3[0].control.ruta == "/upnp/control/WANPPPConn1", s3.empty() ? "sin servicios" : s3[0].control.ruta);
	// SOAP: armar
	const std::string add = upnp::armarAgregar("urn:schemas-upnp-org:service:WANIPConnection:1", 47924, 47924, "192.168.1.20", 7200);
	const std::string pet = upnp::armarPeticionSoap(s1[0].control, s1[0].tipo, "AddPortMapping", add);
	const size_t a = add.find("<NewRemoteHost>"), b = add.find("<NewExternalPort>47924"), c = add.find("<NewProtocol>UDP"),
		d = add.find("<NewInternalPort>47924"), e = add.find("<NewInternalClient>192.168.1.20"), f = add.find("<NewEnabled>1"),
		g = add.find("<NewPortMappingDescription>Yugioh Classic Remaster"), h = add.find("<NewLeaseDuration>7200");
	caso("CU-RED-NAT-SOAP-ARMAR", a < b && b < c && c < d && d < e && e < f && f < g && g < h && h != std::string::npos &&
		pet.find("POST /ctl/IPConn HTTP/1.1\r\n") == 0 && pet.find("SOAPACTION: \"urn:schemas-upnp-org:service:WANIPConnection:1#AddPortMapping\"") != std::string::npos &&
		pet.find("CONTENT-LENGTH: " + std::to_string(add.size())) != std::string::npos,
		"argumentos en el orden del estandar, SOAPACTION y largo");
	// SOAP: leer
	upnp::RespuestaHttp r1, r2, r3;
	const bool l1 = upnp::leerHttp(conLargoReal(SOAP_IP_MINIUPNPD), r1), l2 = upnp::leerHttp(falla718(), r2),
		l3 = upnp::leerHttp(conLargoReal(SOAP_IP_PREFIJO), r3);
	caso("CU-RED-NAT-SOAP-LEER", l1 && r1.estado == 200 && upnp::etiqueta(r1.cuerpo, "NewExternalIPAddress") == "203.0.113.77" &&
		l2 && r2.estado == 500 && upnp::codigoError(r2.cuerpo) == 718 &&
		l3 && upnp::etiqueta(r3.cuerpo, "NewExternalIPAddress") == "100.72.14.3",
		"IP externa (sin y con prefijo u:), falla 718 en chunked");
	upnp::RespuestaHttp r4;
	const std::string ch = chunked(DESC_FRITZ, "HTTP/1.1 200 OK\r\nCONTENT-TYPE: text/xml\r\n");
	caso("CU-RED-NAT-HTTP-CHUNKED", upnp::leerHttp(ch, r4) && r4.cuerpo == DESC_FRITZ && upnp::httpCompleta(ch) &&
		!upnp::httpCompleta(ch.substr(0, ch.size() / 2)), "descripcion en trozos de 100 bytes");
	// NAT-PMP
	uint8_t p[16];
	const size_t np = upnp::armarPmpMapeo(p, 47924, 47924, 7200);
	const auto rp = hex("0081000000001234bb34bb3600001c20");
	const auto ra = hex("008000000000123acb007121");
	uint16_t ext = 0;
	uint32_t dur = 0;
	Direccion ip;
	caso("CU-RED-NAT-NATPMP", np == 12 && p[1] == 1 && p[4] == 0xbb && p[5] == 0x34 && upnp::leerPmpMapeo(rp.data(), rp.size(), 47924, ext, dur) &&
		ext == 47926 && dur == 7200 && upnp::leerPmpDireccion(ra.data(), ra.size(), ip) && soloIp(ip) == "203.0.113.33",
		"mapeo UDP y direccion externa");
}

// ------------------------------------------------------------------------------------- router UPnP de mentira (TCP)
struct RouterFalso {
	detalle::Fd escucha = detalle::FD_NULO;
	uint16_t puerto = 0;
	std::thread hilo;
	std::atomic<bool> parar{ false };
	std::mutex m;
	std::vector<std::string> peticiones;
	std::string ipExterna = "203.0.113.77";
	int ocupadosAntes = 1;   // AddPortMapping devuelve 718 estas veces
	bool chunkedDesc = true;

	bool iniciar()
	{
		escucha = socket(AF_INET, SOCK_STREAM, 0);
		int si = 1;
		setsockopt(escucha, SOL_SOCKET, SO_REUSEADDR, &si, sizeof si);
		sockaddr_in a{};
		a.sin_family = AF_INET;
		a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		if (bind(escucha, (sockaddr*)&a, sizeof a) != 0 || listen(escucha, 8) != 0) return false;
		socklen_t l = sizeof a;
		getsockname(escucha, (sockaddr*)&a, &l);
		puerto = ntohs(a.sin_port);
		hilo = std::thread([this] { bucle(); });
		return true;
	}
	void detener() { parar = true; if (hilo.joinable()) hilo.join(); close(escucha); }
	std::string responder(const std::string& pet)
	{
		const std::string desc = std::string(DESC_MINIUPNPD);
		if (pet.compare(0, 4, "GET ") == 0) {
			if (pet.find("GET /rootDesc.xml") != 0) return "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
			return chunkedDesc ? chunked(desc, "HTTP/1.1 200 OK\r\nContent-Type: text/xml\r\n")
			                   : "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(desc.size()) + "\r\n\r\n" + desc;
		}
		auto soap = [](const std::string& cuerpo) {
			const std::string x = "<?xml version=\"1.0\"?>\r\n<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\"><s:Body>" + cuerpo + "</s:Body></s:Envelope>\r\n";
			return "HTTP/1.1 200 OK\r\nContent-Type: text/xml; charset=\"utf-8\"\r\nContent-Length: " + std::to_string(x.size()) + "\r\n\r\n" + x;
		};
		if (pet.find("#GetExternalIPAddress\"") != std::string::npos)
			return soap("<u:GetExternalIPAddressResponse xmlns:u=\"urn:schemas-upnp-org:service:WANIPConnection:1\"><NewExternalIPAddress>" + ipExterna +
				"</NewExternalIPAddress></u:GetExternalIPAddressResponse>");
		if (pet.find("#AddPortMapping\"") != std::string::npos) {
			if (ocupadosAntes-- > 0) return falla718();
			return soap("<u:AddPortMappingResponse xmlns:u=\"urn:schemas-upnp-org:service:WANIPConnection:1\"/>");
		}
		if (pet.find("#DeletePortMapping\"") != std::string::npos)
			return soap("<u:DeletePortMappingResponse xmlns:u=\"urn:schemas-upnp-org:service:WANIPConnection:1\"/>");
		return "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
	}
	void bucle()
	{
		while (!parar) {
			pollfd p{ escucha, POLLIN, 0 };
			if (poll(&p, 1, 30) <= 0) continue;
			int c = accept(escucha, nullptr, nullptr);
			if (c < 0) continue;
			std::string pet;
			char b[4096];
			const uint64_t fin = ahoraMs() + 1000;
			while (ahoraMs() < fin) {   // cabeceras + Content-Length
				pollfd q{ c, POLLIN, 0 };
				if (poll(&q, 1, 50) <= 0) continue;
				const int n = (int)recv(c, b, sizeof b, 0);
				if (n <= 0) break;
				pet.append(b, (size_t)n);
				const size_t fc = pet.find("\r\n\r\n");
				if (fc == std::string::npos) continue;
				const std::string cl = upnp::cabecera(pet.substr(0, fc + 2), "CONTENT-LENGTH");
				if (pet.size() >= fc + 4 + (size_t)atol(cl.c_str())) break;
			}
			{ std::lock_guard<std::mutex> g(m); peticiones.push_back(pet); }
			const std::string r = responder(pet);
			for (size_t i = 0; i < r.size(); i += 97) send(c, r.data() + i, std::min<size_t>(97, r.size() - i), 0);   // en pedazos
			close(c);
		}
	}
};

static void pruebaUpnpEntero()
{
	RouterFalso rf;
	if (!rf.iniciar()) { caso("CU-RED-NAT-UPNP-FLUJO", false, "no se pudo abrir el router de mentira"); return; }
	const std::vector<std::string> ubic = { "http://127.0.0.1:" + std::to_string(rf.puerto) + "/rootDesc.xml" };
	upnp::Mapeo m;
	const uint64_t t0 = ahoraMs();
	const bool ok = upnp::abrirPuerto(47924, 5000, m, &ubic);
	std::string add2;
	{
		std::lock_guard<std::mutex> g(rf.m);
		for (const std::string& p : rf.peticiones) if (p.find("#AddPortMapping") != std::string::npos) add2 = p;
	}
	caso("CU-RED-NAT-UPNP-FLUJO", ok && m.activo && !m.pmp && soloIp(m.ipExterna) == "203.0.113.77" && m.interno == 47924 && m.externo != 47924 &&
		add2.find("<NewInternalClient>127.0.0.1</NewInternalClient>") != std::string::npos &&
		add2.find("<NewExternalPort>" + std::to_string(m.externo) + "<") != std::string::npos,
		"descripcion en chunked, 718 (ocupado) y luego otro puerto externo: " + std::to_string(m.externo) + " (" + std::to_string(ahoraMs() - t0) + " ms)");
	upnp::liberar();
	bool borro = false;
	{
		std::lock_guard<std::mutex> g(rf.m);
		for (const std::string& p : rf.peticiones)
			borro |= p.find("#DeletePortMapping") != std::string::npos && p.find("<NewExternalPort>" + std::to_string(m.externo) + "<") != std::string::npos;
	}
	caso("CU-RED-NAT-UPNP-LIBERAR", borro && !upnp::mapeoActual().activo, "DeletePortMapping del puerto externo");
	// router detras de CGNAT: la IP externa del router es 100.64/10 -> Cgnat aunque STUN responda
	rf.ipExterna = "100.72.14.3";
	rf.ocupadosAntes = 0;
	rf.chunkedDesc = false;
	upnp::Mapeo m2;
	const bool ok2 = upnp::abrirPuerto(47925, 5000, m2, &ubic);
	const Direccion stunVio = dir("198.51.100.200:61000");
	const std::vector<Direccion> locales = { dir("192.168.1.20") };
	caso("CU-RED-NAT-UPNP-CGNAT", ok2 && m2.externo == 47925 && clasificar(locales, 47925, { stunVio, stunVio }, &m2.ipExterna) == TipoNat::Cgnat,
		"el router dice " + soloIp(m2.ipExterna));
	upnp::liberar();
	rf.detener();
}

// ================================================================================================ codigo manual
static void pruebaCodigo()
{
	auto cand = [](const char* d, TipoCandidato t) { Candidato c; c.dir = dir(d); c.tipo = t; c.prioridad = prioridadTipo(t); return c; };
	const std::vector<Candidato> c1 = { cand("203.0.113.77:47924", TipoCandidato::Upnp), cand("203.0.113.77:51234", TipoCandidato::Stun),
		cand("192.168.1.20:47924", TipoCandidato::Local) };
	const uint64_t id = 0x0123456789ABCDEFull;
	const std::string t = codigoDeCandidatos(c1, id);
	std::vector<Candidato> leidos;
	uint64_t idLeido = 0;
	bool ok = leerCodigo(t, leidos, idLeido);
	// (el orden es el de prioridad: Local, Upnp, Stun)
	auto esta = [&](const Candidato& c) { for (const Candidato& x : leidos) if (x.dir == c.dir && x.tipo == c.tipo) return true; return false; };
	const bool iguales = ok && leidos.size() == 3 && idLeido == id && esta(c1[0]) && esta(c1[1]) && esta(c1[2]) && leidos[0].tipo == TipoCandidato::Local;
	caso("CU-RED-NAT-CODIGO-IDA-VUELTA", iguales, t + " (" + std::to_string(t.size()) + " caracteres)");
	size_t utiles = 0;
	for (char ch : t) utiles += ch != '-';
	caso("CU-RED-NAT-CODIGO-CORTO", utiles <= 48, std::to_string(utiles) + " caracteres (LAN + UPnP + STUN)");
	// con IPv6 y minusculas, sin guiones, O/I confundidas
	const std::vector<Candidato> c2 = { cand("[2001:db8:1:2::77]:47924", TipoCandidato::Ipv6), cand("198.51.100.4:3000", TipoCandidato::Stun) };
	std::string t2 = codigoDeCandidatos(c2, 42), flojo;
	for (char ch : t2) if (ch != '-') flojo += (char)(ch == '0' ? 'o' : ch == '1' ? 'I' : tolower((unsigned char)ch));
	ok = leerCodigo(flojo, leidos, idLeido);
	caso("CU-RED-NAT-CODIGO-IPV6-MINUSCULAS", ok && idLeido == 42 && leidos.size() == 2 && leidos[0].dir == c2[0].dir && leidos[1].dir == c2[1].dir, flojo);
	// errores: un caracter cambiado, truncado, caracter invalido, vacio
	std::string malo = t;
	malo[3] = malo[3] == 'A' ? 'B' : 'A';
	int detectados = 0, total = 0;
	for (size_t i = 0; i < t.size(); i++) {   // cada posicion cambiada
		if (t[i] == '-') continue;
		std::string x = t;
		x[i] = x[i] == 'Z' ? 'Y' : (x[i] == '9' ? 'A' : (char)(x[i] + 1));
		if (x[i] == 'I' || x[i] == 'L' || x[i] == 'O' || x[i] == 'U') x[i]++;
		total++;
		detectados += !leerCodigo(x, leidos, idLeido);
	}
	caso("CU-RED-NAT-CODIGO-ERRORES", !leerCodigo(malo, leidos, idLeido) && !leerCodigo(t.substr(0, t.size() - 5), leidos, idLeido) &&
		!leerCodigo(t + "U", leidos, idLeido) && !leerCodigo("", leidos, idLeido) && !leerCodigo("hola mundo", leidos, idLeido) && detectados == total,
		"cada caracter cambiado detectado: " + std::to_string(detectados) + "/" + std::to_string(total));
	ok = leerCodigo(" 203.0.113.5:47924 ", leidos, idLeido);
	const bool ok2 = ok && leidos.size() == 1 && leidos[0].tipo == TipoCandidato::Manual && leidos[0].dir == dir("203.0.113.5:47924") && idLeido == 0;
	const bool ok3 = leerCodigo("203.0.113.5", leidos, idLeido) && leidos[0].dir.puerto == PUERTO_JUEGO;
	caso("CU-RED-NAT-CODIGO-MANUAL", ok2 && ok3, "direccion escrita a mano (sin puerto: el del juego)");
}

// ================================================================================================ clasificacion
static void pruebaClasificar()
{
	const std::vector<Direccion> lan = { dir("192.168.1.20") };
	const Direccion s1 = dir("203.0.113.77:51234"), s1b = dir("203.0.113.77:51234"), s2 = dir("203.0.113.77:51300");
	const Direccion rCgnat = dir("100.64.0.1"), rCgnatFin = dir("100.127.255.254"), rNoCgnat = dir("100.128.0.1"),
		rPriv = dir("10.20.0.5"), rPub = dir("203.0.113.77"), rOtra = dir("203.0.113.99");
	caso("CU-RED-NAT-RANGOS", esCgnat(rCgnat) && esCgnat(rCgnatFin) && !esCgnat(rNoCgnat) && esPrivada(rPriv) && esPrivada(dir("172.16.0.1")) &&
		esPrivada(dir("172.31.255.1")) && !esPrivada(dir("172.32.0.1")) && esPrivada(dir("192.168.0.1")) && !esPrivada(rPub) &&
		esPrivada(dir("fd00::1")) && esIpv6Global(dir("2001:db8::1")) && !esIpv6Global(dir("fd00::1")) && !esIpv6Global(dir("fe80::1")),
		"100.64/10, 10/8, 172.16/12, 192.168/16, ULA, 2000::/3");
	caso("CU-RED-NAT-CLASIFICA-CGNAT", clasificar(lan, 47924, { s1, s1b }, &rCgnat) == TipoNat::Cgnat &&
		clasificar(lan, 47924, { s1, s1b }, &rPriv) == TipoNat::Cgnat && clasificar(lan, 47924, { s1, s1b }, &rOtra) == TipoNat::Cgnat &&
		clasificar(lan, 47924, {}, &rCgnat) == TipoNat::Cgnat, "router con 100.64/10, privada, o distinta de la de STUN");
	caso("CU-RED-NAT-CLASIFICA-SIMETRICO", clasificar(lan, 47924, { s1, s2 }, &rPub) == TipoNat::Simetrico && clasificar(lan, 47924, { s1, s2 }, nullptr) == TipoNat::Simetrico);
	caso("CU-RED-NAT-CLASIFICA-CONO", clasificar(lan, 47924, { s1, s1b }, &rPub) == TipoNat::Cono && clasificar(lan, 47924, { s1, s1b }, nullptr) == TipoNat::Cono);
	const std::vector<Direccion> publica = { dir("203.0.113.77") };
	caso("CU-RED-NAT-CLASIFICA-OTROS", clasificar(publica, 51234, { s1, s1b }, nullptr) == TipoNat::Abierto &&
		clasificar({}, 47924, {}, nullptr) == TipoNat::SinRed && clasificar(lan, 47924, {}, nullptr) == TipoNat::Desconocido &&
		clasificar(lan, 47924, { s1 }, nullptr) == TipoNat::Desconocido, "abierto, sin red, desconocido");
	InfoRed i;
	i.nat = TipoNat::Cgnat;
	const char* a1 = avisoRed(i);
	i.nat = TipoNat::Simetrico;
	const char* a2 = avisoRed(i);
	i.upnpOk = true;
	const char* a3 = avisoRed(i);
	i.nat = TipoNat::Cono;
	caso("CU-RED-NAT-AVISOS", a1 && strstr(a1, "CGNAT") && a2 && strstr(a2, "simétrico") && !a3 && !avisoRed(i));
	i.nat = TipoNat::SinRed;
	const char* a4 = avisoRed(i);
	bool traducidos = true;
	for (const char* x : { a1, a2, a4, TEXTO_PUERTO_MANUAL }) traducidos = traducidos && x && idioma::tabla().count(x) == 1;
	caso("CU-RED-NAT-TEXTOS-ES-EN", traducidos, "los avisos estan en la tabla de idioma.h");
}

// ================================================================================================ perforacion en loopback
static void pruebaLoopback()
{
	for (int v6 = 0; v6 < 2; v6++) {
		SocketUdp a, b;
		if (!a.abrir(0, v6) || !b.abrir(0, v6) || (v6 && (!a.esV6() || !b.esV6()))) {
			caso(v6 ? "CU-RED-NAT-PERFORAR-LOOPBACK-V6" : "CU-RED-NAT-PERFORAR-LOOPBACK", v6 == 1, "sin IPv6 en el contenedor: omitido");
			continue;
		}
		auto c = [&](const char* ip, uint16_t p, TipoCandidato t) { Candidato x; x.dir = dir(ip); x.dir.puerto = p; x.tipo = t; x.prioridad = prioridadTipo(t); return x; };
		const char* lo = v6 ? "::1" : "127.0.0.1";
		// de cada lado: uno que no responde (TEST-NET), el bueno y uno de otro puerto
		const std::vector<Candidato> deB = { c("192.0.2.1", 9, TipoCandidato::Upnp), c(lo, b.puertoLocal(), TipoCandidato::Local), c(lo, 9, TipoCandidato::Stun) };
		const std::vector<Candidato> deA = { c(lo, a.puertoLocal(), TipoCandidato::Local), c("198.51.100.9", 4000, TipoCandidato::Stun) };
		Direccion ea, eb;
		bool oka = false, okb = false;
		const uint64_t id = nuevoIdSesion(), t0 = ahoraMs();
		std::thread ha([&] { oka = perforar(a, deB, id, 4000, ea); });
		std::thread hb([&] {   // el canal del otro lado manda basura y STUN antes: no confunde la perforacion
			uint8_t basura[40] = { 1, 2, 3 }, st[20];
			const uint8_t tid[12] = {};
			Direccion da = deA[0].dir;
			b.enviar(da, basura, sizeof basura);
			b.enviar(da, st, stun::armarPeticion(st, tid));
			okb = perforar(b, deA, id, 4000, eb);
		});
		ha.join();
		hb.join();
		const uint64_t ms = ahoraMs() - t0;
		bool datos = false;
		if (oka && okb) {   // y despues pasan datos del juego por el camino elegido
			const uint8_t hola[5] = { 'h', 'o', 'l', 'a', 0 };
			a.enviar(ea, hola, 5);
			uint8_t r[64];
			Direccion de;
			for (int k = 0; k < 20 && !datos; k++) {
				const int n = b.recibir(de, r, sizeof r, 50);
				if (n > 0 && esSondeo(r, (size_t)n)) { responderSondeo(b, r, (size_t)n, de, id); continue; }
				datos = n == 5 && memcmp(r, hola, 5) == 0;
			}
		}
		caso(v6 ? "CU-RED-NAT-PERFORAR-LOOPBACK-V6" : "CU-RED-NAT-PERFORAR-LOOPBACK", oka && okb && datos && ea == deB[1].dir && eb == deA[0].dir,
			std::string(oka && okb ? "conecta en " + std::to_string(ms) + " ms: " + ea.texto() + " <-> " + eb.texto() : "no conecta"));
		if (!v6) {   // otro idSesion: no se conecta (es otra partida)
			bool x = true, y = true;
			Direccion e1, e2;
			std::thread h1([&] { x = perforar(a, deB, 111, 1200, e1); });
			std::thread h2([&] { y = perforar(b, deA, 222, 1200, e2); });
			h1.join();
			h2.join();
			caso("CU-RED-NAT-PERFORAR-OTRA-SESION", !x && !y, "sondeos de otra partida ignorados");
		}
	}
}

// ================================================================================================ NAT simulado
// Un "internet" en el proceso: cada maquina tiene un SocketUdp real en loopback que manda sus paquetes al internet
// envueltos con el destino (ip4 + puerto); el internet aplica los NAT (mapeo y filtrado), contesta como servidor
// STUN y entrega envuelto con el origen ya traducido. Asi se prueba nat.h sin permisos de red del contenedor.
enum TipoSim { PUBLICO, CONO_COMPLETO, CONO_IP, CONO_PUERTO, SIMETRICO };
static const char* nombreSim(int t) { static const char* n[] = { "publico", "cono completo", "cono restringido (IP)", "cono restringido (puerto)", "simetrico" }; return n[t]; }

struct NatSim {
	std::string nombre;
	Direccion pub;
	int tipo = CONO_PUERTO;
	bool horquilla = true;
	NatSim* arriba = nullptr;   // CGNAT: el NAT del proveedor
	struct Mapeo { Direccion interna, destino; uint16_t puertoPub; std::vector<Direccion> permisos; };
	std::vector<Mapeo> mapeos;
	uint16_t siguiente = 30000;
};
struct HostSim {
	std::string nombre;
	NatSim* nat = nullptr;
	Direccion priv;       // con NAT: su direccion de la LAN; sin NAT: la publica
	SocketUdp real;
	Direccion realDir;    // 127.0.0.1:<puerto real>
};

class Internet {
 public:
	std::vector<NatSim*> nats;
	std::vector<HostSim*> hosts;
	std::vector<Direccion> stun;   // servidores STUN publicos
	std::atomic<double> perdida{ 0 };
	SocketUdp sock;
	Direccion dirReal;
	std::thread hilo;
	std::atomic<bool> parar{ false };
	std::mt19937 azar{ 7 };
	std::atomic<int> entregados{ 0 };

	bool iniciar()
	{
		if (!sock.abrir(0, false)) return false;
		dirReal = dir("127.0.0.1");
		dirReal.puerto = sock.puertoLocal();
		hilo = std::thread([this] { bucle(); });
		return true;
	}
	void detener() { parar = true; if (hilo.joinable()) hilo.join(); }
	void bucle()
	{
		uint8_t b[2048];
		while (!parar) {
			Direccion de;
			const int n = sock.recibir(de, b, sizeof b, 20);
			if (n < 6) continue;
			for (HostSim* h : hosts)
				if (h->realDir == de) {
					Direccion d;
					memcpy(d.ip, b, 4);
					d.puerto = (uint16_t)((b[4] << 8) | b[5]);
					salir(h->nat, h->priv, d, b + 6, (size_t)n - 6);
				}
		}
	}
	void entregar(HostSim* h, const Direccion& src, const uint8_t* p, size_t n)
	{
		if (perdida > 0 && std::uniform_real_distribution<double>(0, 1)(azar) < perdida) return;
		uint8_t b[2048];
		memcpy(b, src.ip, 4);
		b[4] = (uint8_t)(src.puerto >> 8);
		b[5] = (uint8_t)src.puerto;
		memcpy(b + 6, p, n);
		sock.enviar(h->realDir, b, n + 6);
		entregados++;
	}
	// de adentro de 'nat' (o de internet si es nullptr) hacia d
	void salir(NatSim* nat, const Direccion& src, const Direccion& d, const uint8_t* p, size_t n)
	{
		if (!nat) { publico(src, d, p, n); return; }
		for (HostSim* h : hosts) if (h->nat == nat && h->priv == d) { entregar(h, src, p, n); return; }   // misma LAN
		for (NatSim* c : nats) if (c->arriba == nat && mismaIp(c->pub, d)) { entrar(c, src, d, p, n); return; }   // LAN del CGNAT
		if (!nat->arriba && (esPrivada(d) || esCgnat(d))) {   // privada de otra red: muere en el router (igual abre el mapeo)
			mapeo(nat, src, d);
			return;
		}
		NatSim::Mapeo& m = mapeo(nat, src, d);
		Direccion fuera = nat->pub;
		fuera.puerto = m.puertoPub;
		salir(nat->arriba, fuera, d, p, n);
	}
	NatSim::Mapeo& mapeo(NatSim* nat, const Direccion& src, const Direccion& d)
	{
		for (NatSim::Mapeo& m : nat->mapeos)
			if (m.interna == src && (nat->tipo != SIMETRICO || m.destino == d)) {
				bool ya = false;
				for (const Direccion& x : m.permisos) ya |= x == d;
				if (!ya) m.permisos.push_back(d);
				return m;
			}
		NatSim::Mapeo m;
		m.interna = src;
		m.destino = d;
		m.puertoPub = nat->siguiente;
		nat->siguiente += (uint16_t)(1 + azar() % 7);   // puertos nuevos no predecibles
		m.permisos.push_back(d);
		nat->mapeos.push_back(m);
		return nat->mapeos.back();
	}
	void publico(const Direccion& src, const Direccion& d, const uint8_t* p, size_t n)
	{
		for (const Direccion& s : stun)
			if (s == d) {
				uint8_t tid[12], r[128];
				if (!stun::leerPeticion(p, n, tid)) return;
				const size_t nr = stun::armarRespuesta(r, tid, src);
				publico(s, src, r, nr);
				return;
			}
		for (NatSim* nat : nats) if (!nat->arriba && mismaIp(nat->pub, d)) { entrar(nat, src, d, p, n); return; }
		for (HostSim* h : hosts) if (!h->nat && h->priv == d) { entregar(h, src, p, n); return; }
	}
	// llega a la cara publica de 'nat' (d) desde src
	void entrar(NatSim* nat, const Direccion& src, const Direccion& d, const uint8_t* p, size_t n)
	{
		if (mismaIp(src, nat->pub) && !nat->horquilla) return;
		for (NatSim::Mapeo& m : nat->mapeos) {
			if (m.puertoPub != d.puerto) continue;
			bool pasa = nat->tipo == CONO_COMPLETO;
			for (const Direccion& x : m.permisos)
				pasa |= nat->tipo == CONO_IP ? mismaIp(x, src) : x == src;
			if (!pasa) return;
			for (HostSim* h : hosts) if (h->nat == nat && h->priv == m.interna) { entregar(h, src, p, n); return; }
			for (NatSim* c : nats) if (c->arriba == nat && mismaIp(c->pub, m.interna)) { entrar(c, src, m.interna, p, n); return; }
			return;
		}
	}
};

// el socket de una maquina simulada (misma forma que SocketUdp para las plantillas de nat.h)
struct SocketSim {
	HostSim* h;
	Internet* net;
	bool enviar(const Direccion& a, const uint8_t* datos, size_t n)
	{
		if (a.v6) return false;
		uint8_t b[2048];
		memcpy(b, a.ip, 4);
		b[4] = (uint8_t)(a.puerto >> 8);
		b[5] = (uint8_t)a.puerto;
		memcpy(b + 6, datos, n);
		return h->real.enviar(net->dirReal, b, n + 6);
	}
	int recibir(Direccion& de, uint8_t* buf, size_t cap, int esperaMs)
	{
		uint8_t b[2048];
		Direccion x;
		const int n = h->real.recibir(x, b, sizeof b, esperaMs);
		if (n < 6 || !(x == net->dirReal)) return n < 0 ? n : 0;
		de = Direccion();
		memcpy(de.ip, b, 4);
		de.puerto = (uint16_t)((b[4] << 8) | b[5]);
		const size_t m = std::min(cap, (size_t)n - 6);
		memcpy(buf, b + 6, m);
		return (int)m;
	}
	uint16_t puertoLocal() const { return h->priv.puerto; }
	bool esV6() const { return false; }
};

// tipo del camino elegido; Manual = la direccion reflexiva del par (no estaba en sus candidatos: NAT simetrico)
static const char* nombreCamino(TipoCandidato t) { return t == TipoCandidato::Manual ? "reflexiva" : nombreTipo(t); }
struct ResultadoSim { bool conecta = false, datos = false; TipoNat natA = TipoNat::Desconocido, natB = TipoNat::Desconocido; TipoCandidato tipoA = TipoCandidato::Manual, tipoB = TipoCandidato::Manual; uint64_t ms = 0; };

// dos maquinas A y B (ya puestas en net): reunir, intercambiar (como la sala) y perforar a la vez
static ResultadoSim duelo(Internet& net, HostSim& A, HostSim& B, int esperaMs)
{
	ResultadoSim r;
	SocketSim sa{ &A, &net }, sb{ &B, &net };
	OpcionesReunir op;
	op.usarUpnp = false;
	op.resolverStun = false;
	op.stunFijos = net.stun;
	op.localesFijos = true;
	InfoRed ia, ib;
	std::thread t1([&] { OpcionesReunir o = op; o.locales = { A.priv }; o.locales[0].puerto = 0; ia = reunirCandidatosCon(sa, 3000, o); });
	std::thread t2([&] { OpcionesReunir o = op; o.locales = { B.priv }; o.locales[0].puerto = 0; ib = reunirCandidatosCon(sb, 3000, o); });
	t1.join();
	t2.join();
	r.natA = ia.nat;
	r.natB = ib.nat;
	// la sala pasa los candidatos; aqui ademas por el codigo manual, ida y vuelta
	std::vector<Candidato> deA, deB;
	uint64_t id1 = 0, id2 = 0;
	const uint64_t id = nuevoIdSesion();
	leerCodigo(codigoConexion(ia, id), deA, id1);
	leerCodigo(codigoConexion(ib, id), deB, id2);
	Direccion ea, eb;
	bool oka = false, okb = false;
	const uint64_t t0 = ahoraMs();
	std::thread p1([&] { oka = perforarCon(sa, deB, id1, esperaMs, ea); });
	std::thread p2([&] { okb = perforarCon(sb, deA, id2, esperaMs, eb); });
	p1.join();
	p2.join();
	r.ms = ahoraMs() - t0;
	r.conecta = oka && okb;
	auto tipoDe = [](const std::vector<Candidato>& v, const Direccion& d) { for (const Candidato& c : v) if (c.dir == d) return c.tipo; return TipoCandidato::Manual; };
	r.tipoA = tipoDe(deB, ea);
	r.tipoB = tipoDe(deA, eb);
	if (r.conecta) {   // datos del juego en los dos sentidos por los caminos elegidos
		const double perdida = net.perdida.load();
		net.perdida = 0;
		const uint8_t m1[4] = { 'd', 'e', 'A', 0 }, m2[4] = { 'd', 'e', 'B', 0 };
		bool llegoB = false, llegoA = false;
		for (int k = 0; k < 40 && !(llegoA && llegoB); k++) {
			sa.enviar(ea, m1, 4);
			sb.enviar(eb, m2, 4);
			uint8_t x[64];
			Direccion de;
			int n;
			while ((n = sb.recibir(de, x, sizeof x, 10)) > 0) { if (esSondeo(x, (size_t)n)) responderSondeoCon(sb, x, (size_t)n, de, id2); else llegoB |= n == 4 && memcmp(x, m1, 4) == 0; }
			while ((n = sa.recibir(de, x, sizeof x, 10)) > 0) { if (esSondeo(x, (size_t)n)) responderSondeoCon(sa, x, (size_t)n, de, id1); else llegoA |= n == 4 && memcmp(x, m2, 4) == 0; }
		}
		r.datos = llegoA && llegoB;
		net.perdida = perdida;
	}
	return r;
}

static bool prepararHost(HostSim& h, Internet& net, const char* nombre, NatSim* nat, const char* priv)
{
	h.nombre = nombre;
	h.nat = nat;
	h.priv = dir(priv);
	if (!h.real.abrir(0, false)) return false;
	h.realDir = dir("127.0.0.1");
	h.realDir.puerto = h.real.puertoLocal();
	net.hosts.push_back(&h);
	return true;
}

static bool conectaria(int a, int b)   // lo que dice la teoria
{
	if (a == PUBLICO || b == PUBLICO || a == CONO_COMPLETO || b == CONO_COMPLETO) return true;
	if (a == SIMETRICO || b == SIMETRICO) { const int o = a == SIMETRICO ? b : a; return o == CONO_IP; }
	return true;
}

static void pruebaSimulacion()
{
	printf("\nMatriz de NAT simulados (A x B): conecta / camino elegido por A y B\n");
	int bien = 0, total = 0;
	std::string resumen;
	for (int ta = PUBLICO; ta <= SIMETRICO; ta++)
		for (int tb = ta; tb <= SIMETRICO; tb++) {
			Internet net;
			net.stun = { dir("198.51.100.1:3478"), dir("198.51.100.2:3478") };
			NatSim na, nb;
			na.nombre = "A"; na.pub = dir("203.0.113.10"); na.tipo = ta;
			nb.nombre = "B"; nb.pub = dir("203.0.113.20"); nb.tipo = tb;
			if (ta != PUBLICO) net.nats.push_back(&na);
			if (tb != PUBLICO) net.nats.push_back(&nb);
			HostSim A, B;
			prepararHost(A, net, "A", ta == PUBLICO ? nullptr : &na, ta == PUBLICO ? "203.0.113.10:47924" : "192.168.1.10:47924");
			prepararHost(B, net, "B", tb == PUBLICO ? nullptr : &nb, tb == PUBLICO ? "203.0.113.20:47924" : "192.168.0.50:47924");
			net.iniciar();
			const bool espera = conectaria(ta, tb);
			const ResultadoSim r = duelo(net, A, B, espera ? 6000 : 3000);
			net.detener();
			const TipoNat esperadoA = ta == PUBLICO ? TipoNat::Abierto : ta == SIMETRICO ? TipoNat::Simetrico : TipoNat::Cono;
			const TipoNat esperadoB = tb == PUBLICO ? TipoNat::Abierto : tb == SIMETRICO ? TipoNat::Simetrico : TipoNat::Cono;
			const bool ok = r.conecta == espera && (!r.conecta || r.datos) && r.natA == esperadoA && r.natB == esperadoB;
			total++;
			bien += ok;
			char l[300];
			snprintf(l, sizeof l, "  %-26s x %-26s: %-9s (%s/%s, NAT visto %s/%s, %llu ms)%s\n", nombreSim(ta), nombreSim(tb),
				r.conecta ? (r.datos ? "CONECTA" : "SIN DATOS") : "no", nombreCamino(r.tipoA), nombreCamino(r.tipoB), nombreNat(r.natA), nombreNat(r.natB),
				(unsigned long long)r.ms, ok ? "" : "  <-- distinto de lo esperado");
			printf("%s", l);
			resumen += l;
		}
	caso("CU-RED-NAT-SIM-MATRIZ", bien == total, std::to_string(bien) + "/" + std::to_string(total) + " combinaciones como la teoria");

	// misma casa (los dos detras del mismo router, sin horquilla): por la LAN
	{
		Internet net;
		net.stun = { dir("198.51.100.1:3478"), dir("198.51.100.2:3478") };
		NatSim n;
		n.pub = dir("203.0.113.10"); n.tipo = SIMETRICO; n.horquilla = false;
		net.nats.push_back(&n);
		HostSim A, B;
		prepararHost(A, net, "A", &n, "192.168.1.10:47924");
		prepararHost(B, net, "B", &n, "192.168.1.11:47924");
		net.iniciar();
		const ResultadoSim r = duelo(net, A, B, 6000);
		net.detener();
		caso("CU-RED-NAT-SIM-MISMA-LAN", r.conecta && r.datos && r.tipoA == TipoCandidato::Local && r.tipoB == TipoCandidato::Local,
			std::string("router simetrico sin horquilla: ") + nombreCamino(r.tipoA) + "/" + nombreCamino(r.tipoB) + ", " + std::to_string(r.ms) + " ms");
	}
	// las dos casas usan la misma IP privada (192.168.1.10): el candidato LAN del otro no confunde
	{
		Internet net;
		net.stun = { dir("198.51.100.1:3478"), dir("198.51.100.2:3478") };
		NatSim na, nb;
		na.pub = dir("203.0.113.10"); na.tipo = CONO_PUERTO;
		nb.pub = dir("203.0.113.20"); nb.tipo = CONO_PUERTO;
		net.nats = { &na, &nb };
		HostSim A, B;
		prepararHost(A, net, "A", &na, "192.168.1.10:47924");
		prepararHost(B, net, "B", &nb, "192.168.1.10:47924");
		net.iniciar();
		const ResultadoSim r = duelo(net, A, B, 6000);
		net.detener();
		caso("CU-RED-NAT-SIM-MISMA-IP-PRIVADA", r.conecta && r.datos && r.tipoA == TipoCandidato::Stun && r.tipoB == TipoCandidato::Stun,
			std::string(nombreCamino(r.tipoA)) + "/" + nombreCamino(r.tipoB));
	}
	// CGNAT: casa (cono por puerto) detras del NAT del proveedor (cono = mapeo independiente, como pide el RFC 6888)
	// contra una casa normal; y con un CGNAT simetrico contra cono por puerto (no conecta)
	for (int tipoCgn : { CONO_PUERTO, SIMETRICO }) {
		Internet net;
		net.stun = { dir("198.51.100.1:3478"), dir("198.51.100.2:3478") };
		NatSim cgn, casaA, casaB;
		cgn.pub = dir("203.0.113.100"); cgn.tipo = tipoCgn;
		casaA.pub = dir("100.64.3.7"); casaA.tipo = CONO_PUERTO; casaA.arriba = &cgn;
		casaB.pub = dir("203.0.113.20"); casaB.tipo = CONO_PUERTO;
		net.nats = { &cgn, &casaA, &casaB };
		HostSim A, B;
		prepararHost(A, net, "A", &casaA, "192.168.1.10:47924");
		prepararHost(B, net, "B", &casaB, "192.168.0.50:47924");
		net.iniciar();
		const ResultadoSim r = duelo(net, A, B, tipoCgn == SIMETRICO ? 3000 : 6000);
		net.detener();
		if (tipoCgn == CONO_PUERTO)
			caso("CU-RED-NAT-SIM-CGNAT-CONO", r.conecta && r.datos, std::string("casa + CGNAT cono x casa cono por puerto: ") + (r.conecta ? "conecta por " : "no ") + nombreCamino(r.tipoA));
		else
			caso("CU-RED-NAT-SIM-CGNAT-SIMETRICO", !r.conecta && r.natA == TipoNat::Simetrico, std::string("CGNAT simetrico x cono por puerto: ") +
				(r.conecta ? "conecta" : "no conecta") + ", NAT visto " + nombreNat(r.natA));
	}
	// dos jugadores detras del MISMO CGNAT (con horquilla): por la direccion publica del CGNAT
	{
		Internet net;
		net.stun = { dir("198.51.100.1:3478"), dir("198.51.100.2:3478") };
		NatSim cgn, casaA, casaB;
		cgn.pub = dir("203.0.113.100"); cgn.tipo = CONO_PUERTO; cgn.horquilla = true;
		casaA.pub = dir("100.64.3.7"); casaA.tipo = CONO_PUERTO; casaA.arriba = &cgn;
		casaB.pub = dir("100.64.9.1"); casaB.tipo = CONO_PUERTO; casaB.arriba = &cgn;
		net.nats = { &cgn, &casaA, &casaB };
		HostSim A, B;
		prepararHost(A, net, "A", &casaA, "192.168.1.10:47924");
		prepararHost(B, net, "B", &casaB, "192.168.1.10:47924");
		net.iniciar();
		const ResultadoSim r = duelo(net, A, B, 6000);
		net.detener();
		caso("CU-RED-NAT-SIM-MISMO-CGNAT", r.conecta && r.datos, std::string("horquilla del CGNAT: ") + (r.conecta ? "conecta" : "no conecta"));
	}
	// 20 % de paquetes perdidos (cono por puerto x simetrico no; cono IP x simetrico si)
	{
		Internet net;
		net.stun = { dir("198.51.100.1:3478"), dir("198.51.100.2:3478") };
		net.perdida = 0.2;
		NatSim na, nb;
		na.pub = dir("203.0.113.10"); na.tipo = CONO_IP;
		nb.pub = dir("203.0.113.20"); nb.tipo = SIMETRICO;
		net.nats = { &na, &nb };
		HostSim A, B;
		prepararHost(A, net, "A", &na, "192.168.1.10:47924");
		prepararHost(B, net, "B", &nb, "192.168.0.50:47924");
		net.iniciar();
		const ResultadoSim r = duelo(net, A, B, 8000);
		net.detener();
		caso("CU-RED-NAT-SIM-PERDIDA", r.conecta && r.datos, "20 % de perdida, cono IP x simetrico: " + std::string(r.conecta ? "conecta en " + std::to_string(r.ms) + " ms" : "no conecta"));
	}
}

// ================================================================================================ STUN real (opcional)
static void pruebaReal()
{
	SocketUdp s;
	if (!s.abrir(0, false)) { caso("CU-RED-NAT-STUN-REAL", false, "sin socket"); return; }
	OpcionesReunir op;
	op.usarUpnp = false;
	const uint64_t t0 = ahoraMs();
	InfoRed info = reunirCandidatosCon(s, 4000, op);
	int stunes = 0;
	for (const Candidato& c : info.candidatos) stunes += c.tipo == TipoCandidato::Stun;
	// (no se imprime la IP publica: el log puede terminar en algun lado)
	caso("CU-RED-NAT-STUN-REAL", stunes >= 1 && !info.ipPublica.empty(), std::string("NAT ") + nombreNat(info.nat) + ", " + std::to_string(info.candidatos.size()) +
		" candidatos (" + std::to_string(stunes) + " STUN), " + std::to_string(ahoraMs() - t0) + " ms");
}

int main(int argc, char** argv)
{
	const bool real = argc > 1 && std::string(argv[1]) == "real";
	pruebaStun();
	pruebaDns();
	pruebaSsdpSoap();
	pruebaUpnpEntero();
	pruebaCodigo();
	pruebaClasificar();
	pruebaLoopback();
	pruebaSimulacion();
	if (real) pruebaReal();
	printf("\n%s: %d casos, %d fallos\n", fallos ? "PRUEBA CON FALLOS" : "PRUEBA OK", casos, fallos);
	return fallos ? 1 : 0;
}
