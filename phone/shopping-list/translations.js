// UI strings for the shopping-list phone plugin. Locale follows the system
// language (navigator.language) — French is the default. Supported:
// fr, en, es, it, de, zh-CN.

const STRINGS = {
  fr: {
    appTitle: 'Liste de courses',
    statusStarting: 'Démarrage…',
    statusConnected: 'Lunettes connectées',
    statusDisconnected: 'Lunettes déconnectées',
    startShopping: 'Démarrer les courses',
    itemCount: (n) => `${n} article(s) à acheter`,
    itemPlaceholder: 'Nom de l’article',
    add: 'Ajouter',
    deleteAria: 'Supprimer',
    glassesHint: 'Sur les lunettes : tête haut/bas = élément précédent/suivant, bouton = cocher/décocher l’élément sélectionné.',
    stopShopping: 'Arrêter les courses',
    seedItem1: 'Lait',
    seedItem2: 'Pain',
  },
  en: {
    appTitle: 'Shopping list',
    statusStarting: 'Starting…',
    statusConnected: 'Glasses connected',
    statusDisconnected: 'Glasses disconnected',
    startShopping: 'Start shopping',
    itemCount: (n) => `${n} item(s) to buy`,
    itemPlaceholder: 'Item name',
    add: 'Add',
    deleteAria: 'Delete',
    glassesHint: 'On the glasses: head up/down = previous/next item, button = check/uncheck the selected item.',
    stopShopping: 'Stop shopping',
    seedItem1: 'Milk',
    seedItem2: 'Bread',
  },
  es: {
    appTitle: 'Lista de la compra',
    statusStarting: 'Iniciando…',
    statusConnected: 'Gafas conectadas',
    statusDisconnected: 'Gafas desconectadas',
    startShopping: 'Empezar la compra',
    itemCount: (n) => `${n} artículo(s) por comprar`,
    itemPlaceholder: 'Nombre del artículo',
    add: 'Añadir',
    deleteAria: 'Eliminar',
    glassesHint: 'En las gafas: cabeza arriba/abajo = artículo anterior/siguiente, botón = marcar/desmarcar el artículo seleccionado.',
    stopShopping: 'Detener la compra',
    seedItem1: 'Leche',
    seedItem2: 'Pan',
  },
  it: {
    appTitle: 'Lista della spesa',
    statusStarting: 'Avvio…',
    statusConnected: 'Occhiali connessi',
    statusDisconnected: 'Occhiali disconnessi',
    startShopping: 'Inizia la spesa',
    itemCount: (n) => `${n} articolo/i da comprare`,
    itemPlaceholder: 'Nome dell’articolo',
    add: 'Aggiungi',
    deleteAria: 'Elimina',
    glassesHint: 'Sugli occhiali: testa su/giù = articolo precedente/successivo, pulsante = spunta/togli la spunta all’articolo selezionato.',
    stopShopping: 'Ferma la spesa',
    seedItem1: 'Latte',
    seedItem2: 'Pane',
  },
  de: {
    appTitle: 'Einkaufsliste',
    statusStarting: 'Startet…',
    statusConnected: 'Brille verbunden',
    statusDisconnected: 'Brille getrennt',
    startShopping: 'Einkauf starten',
    itemCount: (n) => `${n} Artikel zu kaufen`,
    itemPlaceholder: 'Artikelname',
    add: 'Hinzufügen',
    deleteAria: 'Löschen',
    glassesHint: 'Auf der Brille: Kopf hoch/runter = vorheriger/nächster Artikel, Taste = ausgewählten Artikel abhaken.',
    stopShopping: 'Einkauf beenden',
    seedItem1: 'Milch',
    seedItem2: 'Brot',
  },
  zh: {
    appTitle: '购物清单',
    statusStarting: '启动中…',
    statusConnected: '眼镜已连接',
    statusDisconnected: '眼镜未连接',
    startShopping: '开始购物',
    itemCount: (n) => `${n} 项待购买`,
    itemPlaceholder: '物品名称',
    add: '添加',
    deleteAria: '删除',
    glassesHint: '在眼镜上：抬头/低头 = 上一项/下一项，按钮 = 勾选/取消勾选所选物品。',
    stopShopping: '结束购物',
    seedItem1: '牛奶',
    seedItem2: '面包',
  },
};

export function detectLocale() {
  const tag = (navigator.language || 'fr').toLowerCase();
  if (tag.startsWith('en')) return 'en';
  if (tag.startsWith('es')) return 'es';
  if (tag.startsWith('it')) return 'it';
  if (tag.startsWith('de')) return 'de';
  if (tag.startsWith('zh')) return 'zh';
  return 'fr';
}

export function getStrings(locale) {
  return STRINGS[locale] || STRINGS.fr;
}
