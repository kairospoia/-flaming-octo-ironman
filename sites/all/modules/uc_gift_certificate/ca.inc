/**
 * Implementation of hook_action().
 */
function uc_gift_certificate_ca_action() {
  $actions['uc_gift_certificate_add_certs'] = array(
    '#title' => t('Add Gift Certificates to an order.'),
    '#callback' => 'uc_gift_certificate_add_certs',
    '#arguments' => array(
      'order' => array('#entity' => 'uc_order', '#title' => t('Order')),
    ),
    '#category' => t('Gift Certificates'),
  );

  return $actions;
}
