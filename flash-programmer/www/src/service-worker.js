import {manifest, version} from '@parcel/service-worker';

async function install() {
    const cache = await caches.open(version);
    await cache.addAll(manifest);
}

addEventListener('install', e => e.waitUntil(install()));

async function activate() {
    const keys = await caches.keys();
    await Promise.all(
        keys.map(key => key !== version && caches.delete(key))
    );
    console.log(`Offline cache version ${version} activated! Floopy Drive is ready for offline access.`);
}

addEventListener('activate', e => e.waitUntil(activate()));

async function fetchCached(request) {
    if (navigator.onLine) {
        return fetch(request);
    }
    if (request.url === self.location.origin + '/') {
        return Response.redirect('/index.html');
    }

    // Offline from cache
    const cache = await caches.open(version);
    const response = await cache.match(request, {ignoreMethod: true, ignoreSearch: true, ignoreVary: true});
    // console.log('Cache version', version, cache, await cache.keys());
    // console.log('Offline', request, response);
    return response || Response.error();
}

addEventListener('fetch', e => e.respondWith(fetchCached(e.request)));
