HISTORY
=======

v0.7 curent trunk
   * add HISTORY.md
   * 777304c change compile format to include in val, isec, ifvar
   * 55de127 support {{../}} in {{#each}}
   * 57e90af fix parent levels detection bug
   * 96bb4d7 fix bugs for {{#.}} and {{#this}}
   * f4217d1 add ifv and unl 2 new methods for LCRun
   * 3f1014c fix {{#this}} and {{#.}} bug when used with {{../var}}
   * cbf0b28 fix {{#if}} logic error when using {{../}}
   * 2b20ef8 fix {{#with}} + {{../}} bug
   * 540cd44 now support FLAG_STANDALONE
   * 67ac5ff support {{>partial}}

v0.6 https://github.com/zordius/lightncandy/tree/v0.6
   * align with handlebarsjs 1.0.11
   * 45ac3b6 fix #with bug when var is false
   * 1a46c2c minor #with logic fix. update document
   * fdc753b fix #each and section logic for 018-hb-withwith-006
   * e6cc95a add FLAG_PARENT, detect template error when scan()
   * 1980691 make new LCRun::val() method to normal path.val logic
   * 110d24f {{#if path.var}} bug fixed
   * d6ae2e6 fix {{#with path.val}} when input value is null
   * 71cf074 fix for 020-hb-doteach testcase

v0.5 https://github.com/zordius/lightncandy/tree/v0.5
   * 955aadf fix #each bug when input is a hash
   * final version for following handlebarsjs 1.0.7
