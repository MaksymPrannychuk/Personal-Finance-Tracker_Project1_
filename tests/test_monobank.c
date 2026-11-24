#include "../app/monobank.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    MonoRate rates[MONO_MAX_RATES]; size_t n;
    assert(mono_parse_rates("[{\"currencyCodeA\":840,\"currencyCodeB\":980,\"date\":1795132800,\"rateBuy\":41.2,\"rateSell\":41.8},{\"currencyCodeA\":978,\"currencyCodeB\":980,\"date\":1795132800,\"rateBuy\":49,\"rateSell\":50},{\"currencyCodeA\":985,\"currencyCodeB\":980,\"date\":1795132800,\"rateCross\":11.2}]",rates,&n)==MONO_OK);
    assert(n==3 && rates[0].currency==840 && rates[0].buy==41.2 && rates[1].currency==978 && rates[2].cross==11.2);
    puts("[PASS] Public rates: USD, EUR and PLN parsing");
    assert(mono_parse_rates("[{\"currencyCodeA\":840,\"currencyCodeB\":980,\"date\":1,\"rateBuy\":-1}]",rates,&n)==MONO_FORMAT && n==0);
    assert(mono_parse_rates("[] trailing",rates,&n)==MONO_FORMAT && n==0);
    assert(mono_parse_rates("[]",rates,&n)==MONO_FORMAT && n==0);
    puts("[PASS] Public rates: malformed JSON, empty data and invalid rates");
    assert(mono_parse_rates("[{\"currencyCodeA\":999,\"currencyCodeB\":980},{\"currencyCodeA\":840,\"currencyCodeB\":980,\"date\":1,\"rateBuy\":42},{\"currencyCodeA\":840,\"currencyCodeB\":980,\"date\":2,\"rateBuy\":43}]",rates,&n)==MONO_OK);
    assert(n==1 && rates[0].buy==42);
    puts("[PASS] Public rates: unsupported currency filtering and duplicate handling");
    assert(mono_http_status(200)==MONO_OK && mono_http_status(403)==MONO_AUTH && mono_http_status(429)==MONO_RATE_LIMIT && mono_http_status(500)==MONO_HTTP && mono_http_status(302)==MONO_HTTP);
    assert(mono_get_rates(NULL,&n)==MONO_INVALID);
    assert(mono_get_rates(rates,NULL)==MONO_INVALID);
    puts("[PASS] Public rates: HTTP error mapping and invalid arguments");
    Transaction original={0}, updated={0};
    strcpy(original.description,"[mono:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef] Groceries");
    strcpy(updated.description,"Updated groceries");
    assert(mono_has_ref(original.description));
    assert(!strcmp(mono_visible_description(original.description),"Groceries"));
    assert(mono_preserve_ref(&updated,&original));
    assert(!strcmp(mono_visible_description(updated.description),"Updated groceries"));
    assert(!mono_has_ref("ordinary local description"));
    puts("[PASS] Compatibility: descriptions of previously imported records");
    puts("All 5 public-rate and compatibility tests passed (offline fixtures).");
    return 0;
}
